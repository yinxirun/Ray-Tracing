#include "vulkan_memory.h"
#include "device.h"
#include "configuration.h"
#include "../definitions.h"
#include <algorithm>
#include "util.h"
#include "queue.h"

// This 'frame number' should only be used for the deletion queue
uint32 GVulkanRHIDeletionFrameNumber = 0;
const uint32 NUM_FRAMES_TO_WAIT_FOR_RESOURCE_DELETE = 2;

namespace VulkanRHI
{
    StagingBuffer::StagingBuffer(Device *InDevice) : device(InDevice)
    {
    }

    VkBuffer StagingBuffer::GetHandle() const { return buffer; }

    void *StagingBuffer::GetMappedPointer()
    {
        void *data;
        vmaMapMemory(device->GetAllocator(), allocation, &data);
        return data;
    }

    uint32 StagingBuffer::GetSize() const { return allocationInfo.size; }

    StagingBuffer::~StagingBuffer() { vmaDestroyBuffer(device->GetAllocator(), buffer, allocation); }

    void StagingBuffer::Destroy()
    {
        vmaDestroyBuffer(device->GetAllocator(), buffer, allocation);
        buffer = VK_NULL_HANDLE;
    }

    StagingManager::~StagingManager()
    {
        check(UsedStagingBuffers.size() == 0);
    }

    void StagingManager::Deinit()
    {
    }

    StagingBuffer *StagingManager::AcquireBuffer(uint32 Size,
                                                 VkBufferUsageFlags InUsageFlags,
                                                 VkMemoryPropertyFlagBits InMemoryReadFlags)
    {
        const bool IsHostCached = (InMemoryReadFlags == VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
        if (IsHostCached)
        {
            printf("Don't support host cache %s %d\n", __FILE__, __LINE__);
            exit(-1);
            // Size = AlignArbitrary(Size, (uint32)device->GetLimits().nonCoherentAtomSize);
        }

        // Add both source and dest flags
        if ((InUsageFlags & (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)) != 0)
        {
            InUsageFlags |= (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        }

        printf("Notice: Don't support Descriptor Buffer %s %d\n", __FILE__, __LINE__);

        StagingBuffer *stagingBuffer = new StagingBuffer(device);
        VkBufferCreateInfo StagingBufferCreateInfo;
        ZeroVulkanStruct(StagingBufferCreateInfo, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
        StagingBufferCreateInfo.size = Size;
        StagingBufferCreateInfo.usage = InUsageFlags;

        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = VMA_MEMORY_USAGE_AUTO;
        allocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        vmaCreateBuffer(device->GetAllocator(), &StagingBufferCreateInfo, &allocCI,
                        &stagingBuffer->buffer, &stagingBuffer->allocation, &stagingBuffer->allocationInfo);

        return stagingBuffer;
    }

    // Sets pointer to nullptr
    void StagingManager::ReleaseBuffer(CmdBuffer *CmdBuffer, StagingBuffer *&StagingBuffer)
    {
        delete StagingBuffer;
        StagingBuffer = nullptr;
    }

    Fence::Fence(Device *InDevice, FenceManager *InOwner, bool bCreateSignaled)
        : state(bCreateSignaled ? Fence::EState::Signaled : Fence::EState::NotReady), owner(InOwner)
    {
        VkFenceCreateInfo Info;
        ZeroVulkanStruct(Info, VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
        Info.flags = bCreateSignaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
        vkCreateFence(InDevice->GetInstanceHandle(), &Info, VULKAN_CPU_ALLOCATOR, &handle);
    }

    Fence::~Fence()
    {
        if (handle != VK_NULL_HANDLE)
            printf("Didn't get properly destroyed by FFenceManager!\n");
    }

    FenceManager::~FenceManager()
    {
        ensure(usedFences.size() == 0);
    }

    void FenceManager::Init(Device *InDevice) { device = InDevice; }

    void FenceManager::Deinit()
    {
        // FScopeLock Lock(&FenceLock);
        // ensureMsgf(UsedFences.Num() == 0, TEXT("No all fences are done!"));
        assert(usedFences.size() == 0);
        VkDevice DeviceHandle = device->GetInstanceHandle();
        for (Fence *fence : freeFences)
        {
            DestroyFence(fence);
        }
    }

    Fence *FenceManager::AllocateFence(bool bCreateSignaled)
    {
        // FScopeLock Lock(&FenceLock);
        if (freeFences.size() != 0)
        {
            Fence *fence = freeFences[0];
            freeFences[0] = freeFences.back();
            freeFences.pop_back();
            // freeFences.RemoveAtSwap(0, 1, EAllowShrinking::No);
            usedFences.push_back(fence);

            if (bCreateSignaled)
            {
                fence->state = Fence::EState::Signaled;
            }
            return fence;
        }

        Fence *NewFence = new Fence(device, this, bCreateSignaled);
        usedFences.push_back(NewFence);
        return NewFence;
    }

    // Returns false if it timed out
    bool FenceManager::WaitForFence(Fence *fence, uint64 timeInNanoseconds)
    {
#if VULKAN_ENABLE_AGGRESSIVE_STATS
        SCOPE_CYCLE_COUNTER(STAT_VulkanWaitFence);
#endif
        bool contain = false;
        for (auto f : usedFences)
        {
            if (f == fence)
            {
                contain = true;
                break;
            }
        }
        check(contain);
        check(fence->state == Fence::EState::NotReady);
        VkResult Result = vkWaitForFences(device->GetInstanceHandle(), 1, &fence->handle, true, timeInNanoseconds);
        switch (Result)
        {
        case VK_SUCCESS:
            fence->state = Fence::EState::Signaled;
            return true;
        case VK_TIMEOUT:
            break;
        default:
            VERIFYVULKANRESULT(Result);
            break;
        }

        return false;
    }

    void FenceManager::ResetFence(Fence *fence)
    {
        if (fence->state != Fence::EState::NotReady)
        {
            VERIFYVULKANRESULT(vkResetFences(device->GetInstanceHandle(), 1, &fence->handle));
            fence->state = Fence::EState::NotReady;
        }
    }

    // Sets it to nullptr
    void FenceManager::ReleaseFence(Fence *&fence)
    {
        // FScopeLock Lock(&FenceLock);
        ResetFence(fence);
        auto iter = std::find(usedFences.begin(), usedFences.end(), fence);
        usedFences.erase(iter);
#if VULKAN_REUSE_FENCES
        freeFences.push_back(fence);
#else
        DestroyFence(Fence);
#endif
        fence = nullptr;
    }

    // Sets it to nullptr
    void FenceManager::WaitAndReleaseFence(Fence *&fence, uint64 timeInNanoseconds)
    {
        if (!fence->IsSignaled())
        {
            WaitForFence(fence, timeInNanoseconds);
        }

        ResetFence(fence);
        auto iter = std::find(usedFences.begin(), usedFences.end(), fence);
        usedFences.erase(iter);
        freeFences.push_back(fence);
        fence = nullptr;
    }

    bool FenceManager::CheckFenceState(Fence *fence)
    {
        check(std::count(usedFences.begin(), usedFences.end(), fence));
        check(fence->state == Fence::EState::NotReady);
        VkResult Result = vkGetFenceStatus(device->GetInstanceHandle(), fence->handle);
        switch (Result)
        {
        case VK_SUCCESS:
            fence->state = Fence::EState::Signaled;
            return true;

        case VK_NOT_READY:
            break;

        default:
            VERIFYVULKANRESULT(Result);
            break;
        }
        return false;
    }

    void FenceManager::DestroyFence(Fence *fence)
    {
        // Does not need to go in the deferred deletion queue
        vkDestroyFence(device->GetInstanceHandle(), fence->GetHandle(), VULKAN_CPU_ALLOCATOR);
        fence->handle = VK_NULL_HANDLE;
        delete fence;
    }

    Semaphore::Semaphore(Device &InDevice) : device(InDevice),
                                             semaphoreHandle(VK_NULL_HANDLE),
                                             bExternallyOwned(false)
    {
        // Create semaphore
        VkSemaphoreCreateInfo CreateInfo;
        ZeroVulkanStruct(CreateInfo, VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
        vkCreateSemaphore(device.GetInstanceHandle(), &CreateInfo, VULKAN_CPU_ALLOCATOR, &semaphoreHandle);
    }

    Semaphore::Semaphore(Device &InDevice, const VkSemaphore &InExternalSemaphore) : device(InDevice),
                                                                                     semaphoreHandle(InExternalSemaphore),
                                                                                     bExternallyOwned(true)
    {
    }

    Semaphore::~Semaphore()
    {
        check(semaphoreHandle != VK_NULL_HANDLE);
        if (!bExternallyOwned)
        {
            device.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::DeferredDeletionQueue2::EType::Semaphore, semaphoreHandle);
        }
        semaphoreHandle = VK_NULL_HANDLE;
    }

    DeferredDeletionQueue2::DeferredDeletionQueue2(Device *InDevice) : DeviceChild(InDevice) {}

    DeferredDeletionQueue2::~DeferredDeletionQueue2() { check(Entries.size() == 0); }

    void DeferredDeletionQueue2::ReleaseResources(bool bDeleteImmediately)
    {
        VkDevice DeviceHandle = device->GetInstanceHandle();

        // Traverse list backwards so the swap switches to elements already tested
        for (int32 Index = Entries.size() - 1; Index >= 0; --Index)
        {
            Entry *entry = &Entries[Index];
            // #todo-rco: Had to add this check, we were getting null CmdBuffers on the first frame, or before first frame maybe
            if (bDeleteImmediately ||
                (GVulkanRHIDeletionFrameNumber > entry->FrameNumber + NUM_FRAMES_TO_WAIT_FOR_RESOURCE_DELETE &&
                 (entry->CmdBuffer == nullptr || entry->FenceCounter < entry->CmdBuffer->GetFenceSignaledCounterC())))
            {
                switch (entry->StructureType)
                {

#define VKSWITCH(Type, ...)                                                           \
    case EType::Type:                                                                 \
        __VA_ARGS__;                                                                  \
        vkDestroy##Type(DeviceHandle, (Vk##Type)entry->Handle, VULKAN_CPU_ALLOCATOR); \
        break

                    VKSWITCH(RenderPass);
                    VKSWITCH(Buffer);
                    VKSWITCH(BufferView);
                    VKSWITCH(Image);
                    VKSWITCH(ImageView);
                    // VKSWITCH(Pipeline, DEC_DWORD_STAT(STAT_VulkanNumPSOs));
                    VKSWITCH(Pipeline);
                    VKSWITCH(PipelineLayout);
                    VKSWITCH(Framebuffer);
                    VKSWITCH(DescriptorSetLayout);
                    VKSWITCH(Sampler);
                    VKSWITCH(Semaphore);
                    VKSWITCH(ShaderModule);
                    VKSWITCH(Event);
#undef VKSWITCH
                case EType::ResourceAllocation:
                {
                    // FVulkanAllocation Allocation = Entry->Allocation;
                    // Allocation.Own();
                    // Device->GetMemoryManager().FreeVulkanAllocation(Allocation, EVulkanFreeFlag_DontDefer);
                    break;
                }
                case EType::DeviceMemoryAllocation:
                {
                    // check(entry->DeviceMemoryAllocation);
                    // device->GetDeviceMemoryManager().Free(entry->DeviceMemoryAllocation);
                    break;
                }
#if VULKAN_RHI_RAYTRACING
                case EType::AccelerationStructure:
                {
                    VulkanDynamicAPI::vkDestroyAccelerationStructureKHR(DeviceHandle, (VkAccelerationStructureKHR)Entry->Handle, VULKAN_CPU_ALLOCATOR);
                    break;
                }
#endif // VULKAN_RHI_RAYTRACING
                case EType::BindlessHandle:
                {
                    check(device->SupportsBindless());
                    // uint32 BindlessType = (uint32)(Entry->Handle >> 32);
                    // check(BindlessType < MAX_uint8);
                    // uint32 BindlessIndex = (uint32)(Entry->Handle & 0xFFFFFFFF);
                    // FRHIDescriptorHandle DescriptorHandle((uint8)BindlessType, BindlessIndex);
                    // Device->GetBindlessDescriptorManager()->Unregister(DescriptorHandle);
                    // break;
                }

                default:
                    check(0);
                    break;
                }
                Entries[Index] = Entries.back();
                Entries.pop_back();
                // Entries.RemoveAtSwap(Index, 1, EAllowShrinking::No);
            }
        }
    }

    void DeferredDeletionQueue2::EnqueueGenericResource(EType Type, uint64 Handle)
    {
        Queue *queue = device->GetGraphicsQueue();

        Entry Entry;
        Entry.StructureType = Type;
        queue->GetLastSubmittedInfo(Entry.CmdBuffer, Entry.FenceCounter);
        Entry.FrameNumber = GVulkanRHIDeletionFrameNumber;
        // Entry.DeviceMemoryAllocation = 0;
        Entry.Handle = Handle;
        {
            // FScopeLock ScopeLock(&CS);

#if VULKAN_HAS_DEBUGGING_ENABLED
            FEntry *ExistingEntry = Entries.FindByPredicate([&](const FEntry &InEntry)
                                                            { return (InEntry.Handle == Entry.Handle) && (InEntry.StructureType == Entry.StructureType); });
            checkf(ExistingEntry == nullptr, TEXT("Attempt to double-delete resource, FDeferredDeletionQueue2::EType: %d, Handle: %llu"), (int32)Type, Handle);
#endif

            Entries.push_back(Entry);
        }
    }
}