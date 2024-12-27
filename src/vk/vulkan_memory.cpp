#include "vulkan_memory.h"
#include "device.h"
#include "configuration.h"
#include "../definitions.h"
#include <algorithm>
#include "util.h"
#include "queue.h"

extern uint32 GFrameNumberRenderThread;

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
        if (mappedPointer == nullptr)
            vmaMapMemory(device->GetAllocator(), allocation, &mappedPointer);
        return mappedPointer;
    }

    uint32 StagingBuffer::GetSize() const { return BufferSize; }

    void StagingBuffer::FlushMappedMemory()
    {
        vmaFlushAllocation(device->GetAllocator(), allocation, 0, VK_WHOLE_SIZE);
    }

    StagingBuffer::~StagingBuffer()
    {
        if (buffer != VK_NULL_HANDLE)
        {
            vmaUnmapMemory(device->GetAllocator(), allocation);
            vmaDestroyBuffer(device->GetAllocator(), buffer, allocation);
#ifdef DEBUG_BUFFER_CREATE_DESTROY
            std::cout << "Destroy StagingBuffer: " << std::hex << buffer << std::endl;
#endif
        }
    }

    void StagingBuffer::Destroy()
    {
        vmaUnmapMemory(device->GetAllocator(), allocation);
        vmaDestroyBuffer(device->GetAllocator(), buffer, allocation);
#ifdef DEBUG_BUFFER_CREATE_DESTROY
        std::cout << "Destroy StagingBuffer: " << std::hex << buffer << std::endl;
#endif
        buffer = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }

    StagingManager::~StagingManager()
    {
        check(usedStagingBuffers.size() == 0);
        check(pendingFreeStagingBuffers.size() == 0);
        check(freeStagingBuffers.size() == 0);
    }

    void StagingManager::Deinit()
    {
        ProcessPendingFree(true, true);
        check(usedStagingBuffers.size() == 0);
        check(pendingFreeStagingBuffers.size() == 0);
        check(freeStagingBuffers.size() == 0);
    }

    StagingBuffer *StagingManager::AcquireBuffer(uint32 size,
                                                 VkBufferUsageFlags InUsageFlags,
                                                 VkMemoryPropertyFlagBits InMemoryReadFlags)
    {
        const bool isHostCached = (InMemoryReadFlags == VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
        if (isHostCached)
        {
            size = AlignArbitrary(size, (uint32)device->GetLimits().nonCoherentAtomSize);
        }

        // Add both source and dest flags
        if ((InUsageFlags & (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)) != 0)
        {
            InUsageFlags |= (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        }

#ifdef PRINT_UNIMPLEMENT
        printf("Notice: Don't support Descriptor Buffer %s %d\n", __FILE__, __LINE__);
#endif

        // #todo-rco: Better locking!
        {
            /* FScopeLock Lock(&StagingLock); */
            for (int32 Index = 0; Index < freeStagingBuffers.size(); ++Index)
            {
                FreeEntry &FreeBuffer = freeStagingBuffers[Index];
                if (FreeBuffer.StagingBuffer->GetSize() == size && FreeBuffer.StagingBuffer->MemoryReadFlags == InMemoryReadFlags)
                {
                    StagingBuffer *Buffer = FreeBuffer.StagingBuffer;
                    freeStagingBuffers[Index] = freeStagingBuffers.back();
                    freeStagingBuffers.pop_back();
                    usedStagingBuffers.push_back(Buffer);
                    return Buffer;
                }
            }
        }

        StagingBuffer *stagingBuffer = new StagingBuffer(device);
        stagingBuffer->MemoryReadFlags = InMemoryReadFlags;
        stagingBuffer->BufferSize = size;

        VkBufferCreateInfo StagingBufferCreateInfo;
        ZeroVulkanStruct(StagingBufferCreateInfo, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
        StagingBufferCreateInfo.size = size;
        StagingBufferCreateInfo.usage = InUsageFlags;

        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = VMA_MEMORY_USAGE_AUTO;
        allocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        vmaCreateBuffer(device->GetAllocator(), &StagingBufferCreateInfo, &allocCI,
                        &stagingBuffer->buffer, &stagingBuffer->allocation, &stagingBuffer->allocationInfo);
#ifdef DEBUG_BUFFER_CREATE_DESTROY
        std::cout << "Create Staging Buffer: " << std::hex << stagingBuffer->buffer << std::endl;
#endif
        {
            /* FScopeLock Lock(&StagingLock); */
            usedStagingBuffers.push_back(stagingBuffer);
            usedMemory += stagingBuffer->GetSize();
            peakUsedMemory = std::max(usedMemory, peakUsedMemory);
        }

        return stagingBuffer;
    }

    // Sets pointer to nullptr
    void StagingManager::ReleaseBuffer(CmdBuffer *CmdBuffer, StagingBuffer *&stagingBuffer)
    {
        /* FScopeLock Lock(&StagingLock); */
        for (int i = 0; i < usedStagingBuffers.size(); ++i)
        {
            if (usedStagingBuffers[i] == stagingBuffer)
            {
                usedStagingBuffers[i] = usedStagingBuffers.back();
                usedStagingBuffers.pop_back();
                break;
            }
        }

        if (CmdBuffer)
        {
            PendingItemsPerCmdBuffer *ItemsForCmdBuffer = FindOrAdd(CmdBuffer);
            PendingItemsPerCmdBuffer::PendingItems *ItemsForFence = ItemsForCmdBuffer->FindOrAddItemsForFence(CmdBuffer->GetFenceSignaledCounter());
            check(stagingBuffer);
            ItemsForFence->Resources.push_back(stagingBuffer);
        }
        else
        {
            freeStagingBuffers.push_back({stagingBuffer, GFrameNumberRenderThread});
        }
        stagingBuffer = nullptr;
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

    TempFrameAllocationBuffer::TempFrameAllocationBuffer(Device *InDevice)
        : DeviceChild(InDevice), BufferIndex(0)
    {
        for (int32 Index = 0; Index < NUM_BUFFERS; ++Index)
        {
            Entries[Index].InitBuffer(device, ALLOCATION_SIZE);
        }
    }
    TempFrameAllocationBuffer::~TempFrameAllocationBuffer()
    {
        Destroy();
    }
    void TempFrameAllocationBuffer::FrameEntry::InitBuffer(Device *InDevice, uint32 InSize)
    {
        Size = InSize;
        PeakUsed = 0;

        VkBufferCreateInfo bufferCI{};
        bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCI.size = Size;
        bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                         VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VmaAllocationCreateInfo allocationCI{};
        allocationCI.usage = VMA_MEMORY_USAGE_AUTO;
        allocationCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocationInfo info{};
        check(buffer == VK_NULL_HANDLE);
        VkResult result = vmaCreateBuffer(InDevice->GetAllocator(), &bufferCI, &allocationCI, &buffer, &allocation, &info);
#ifdef DEBUG_BUFFER_CREATE_DESTROY
        std::cout << "Create TempFrameAllocationBuffer: " << std::hex << buffer << std::endl;
#endif
        if (result == VK_SUCCESS)
        {
            MappedData = (uint8 *)info.pMappedData;
            CurrentData = MappedData;
        }
        else
        {
            throw std::runtime_error("Fail to allocate Temp Frame Buffer.");
        }
    }
    void TempFrameAllocationBuffer::Destroy()
    {
        for (int32 Index = 0; Index < NUM_BUFFERS; ++Index)
        {
            Entries[Index].Reset(device);
            vmaDestroyBuffer(device->GetAllocator(), Entries[Index].buffer, Entries[Index].allocation);
#ifdef DEBUG_BUFFER_CREATE_DESTROY
            std::cout << "Destroy TempFrameAllocationBuffer: " << std::hex << Entries[Index].buffer << std::endl;
#endif
            Entries[Index].buffer = VK_NULL_HANDLE;
            Entries[Index].allocation = VK_NULL_HANDLE;
        }
    }
    bool TempFrameAllocationBuffer::FrameEntry::TryAlloc(uint32 InSize, uint32 InAlignment, TempAllocInfo &OutInfo)
    {
        uint8 *AlignedData = (uint8 *)Align((uintptr_t)CurrentData, (uintptr_t)InAlignment);
        if (AlignedData + InSize <= MappedData + Size)
        {
            OutInfo.Data = AlignedData;
            OutInfo.allocation = allocation;
            OutInfo.buffer = buffer;
            OutInfo.CurrentOffset = (uint32)(AlignedData - MappedData);
            OutInfo.Size = InSize;
            CurrentData = AlignedData + InSize;
            PeakUsed = std::max(PeakUsed, (uint32)(CurrentData - MappedData));
            return true;
        }
        return false;
    }
    void TempFrameAllocationBuffer::Alloc(uint32 InSize, uint32 InAlignment, TempAllocInfo &OutInfo)
    {
        /* FScopeLock ScopeLock(&CS); */

        if (Entries[BufferIndex].TryAlloc(InSize, InAlignment, OutInfo))
        {
            return;
        }

        // Couldn't fit in the current buffers; allocate a new bigger one and schedule the current one for deletion
        uint32 NewSize = Align(ALLOCATION_SIZE + InSize + InAlignment, ALLOCATION_SIZE);

        Entries[BufferIndex].pendingDeletionAlloc.push_back({});
        Entries[BufferIndex].pendingDeletionBuf.push_back({});
        VmaAllocation &PendingDeleteAlloc = Entries[BufferIndex].pendingDeletionAlloc.back();
        VkBuffer &PendingDeleteBuf = Entries[BufferIndex].pendingDeletionBuf.back();
        std::swap(Entries[BufferIndex].allocation, PendingDeleteAlloc);
        std::swap(Entries[BufferIndex].buffer, PendingDeleteBuf);
        Entries[BufferIndex].InitBuffer(device, NewSize);
        if (!Entries[BufferIndex].TryAlloc(InSize, InAlignment, OutInfo))
        {
            check(0);
        }
    }
    void TempFrameAllocationBuffer::Reset()
    {
        /* FScopeLock ScopeLock(&CS); */
        BufferIndex = (BufferIndex + 1) % NUM_BUFFERS;
        Entries[BufferIndex].Reset(device);
    }
    void TempFrameAllocationBuffer::FrameEntry::Reset(Device *InDevice)
    {
        CurrentData = MappedData;
        for (int i = 0; i < pendingDeletionAlloc.size(); ++i)
        {
            vmaDestroyBuffer(InDevice->GetAllocator(), pendingDeletionBuf[i], pendingDeletionAlloc[i]);
#ifdef DEBUG_BUFFER_CREATE_DESTROY
            std::cout << "Destroy TempFrameAllocationBuffer: " << std::hex << pendingDeletionBuf[i] << std::endl;
#endif
        }
        pendingDeletionBuf.clear();
        pendingDeletionAlloc.clear();
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
                 (entry->CmdBuffer == nullptr || entry->FenceCounter < entry->CmdBuffer->GetFenceSignaledCounter())))
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

    void DeferredDeletionQueue2::OnCmdBufferDeleted(CmdBuffer *DeletedCmdBuffer)
    {
        for (int32 Index = 0; Index < Entries.size(); ++Index)
        {
            Entry &entry = Entries[Index];
            if (entry.CmdBuffer == DeletedCmdBuffer)
            {
                entry.CmdBuffer = nullptr;
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

    // 4388
    inline StagingManager::PendingItemsPerCmdBuffer *StagingManager::FindOrAdd(CmdBuffer *CmdBuffer)
    {
        for (int32 Index = 0; Index < pendingFreeStagingBuffers.size(); ++Index)
        {
            if (pendingFreeStagingBuffers[Index].cmdBuffer == CmdBuffer)
            {
                return &pendingFreeStagingBuffers[Index];
            }
        }

        pendingFreeStagingBuffers.push_back(PendingItemsPerCmdBuffer());
        PendingItemsPerCmdBuffer &New = pendingFreeStagingBuffers.back();
        New.cmdBuffer = CmdBuffer;
        return &New;
    }

    // 4403
    inline StagingManager::PendingItemsPerCmdBuffer::PendingItems *StagingManager::PendingItemsPerCmdBuffer::FindOrAddItemsForFence(uint64 Fence)
    {
        for (int32 Index = 0; Index < pendingItems.size(); ++Index)
        {
            if (pendingItems[Index].FenceCounter == Fence)
            {
                return &pendingItems[Index];
            }
        }

        pendingItems.push_back(PendingItems());
        PendingItems &New = pendingItems.back();
        New.FenceCounter = Fence;
        return &New;
    }

    // 4513
    void StagingManager::ProcessPendingFreeNoLock(bool bImmediately, bool bFreeToOS)
    {
        int32 NumOriginalFreeBuffers = freeStagingBuffers.size();
        for (int32 Index = pendingFreeStagingBuffers.size() - 1; Index >= 0; --Index)
        {
            PendingItemsPerCmdBuffer &EntriesPerCmdBuffer = pendingFreeStagingBuffers[Index];
            for (int32 FenceIndex = EntriesPerCmdBuffer.pendingItems.size() - 1; FenceIndex >= 0; --FenceIndex)
            {
                PendingItemsPerCmdBuffer::PendingItems &pendingItems = EntriesPerCmdBuffer.pendingItems[FenceIndex];
                if (bImmediately || pendingItems.FenceCounter < EntriesPerCmdBuffer.cmdBuffer->GetFenceSignaledCounter())
                {
                    for (int32 ResourceIndex = 0; ResourceIndex < pendingItems.Resources.size(); ++ResourceIndex)
                    {
                        check(pendingItems.Resources[ResourceIndex]);
                        freeStagingBuffers.push_back({pendingItems.Resources[ResourceIndex], GFrameNumberRenderThread});
                    }

                    EntriesPerCmdBuffer.pendingItems[FenceIndex] = EntriesPerCmdBuffer.pendingItems.back();
                    EntriesPerCmdBuffer.pendingItems.pop_back();
                }
            }

            if (EntriesPerCmdBuffer.pendingItems.size() == 0)
            {
                pendingFreeStagingBuffers[Index] = pendingFreeStagingBuffers.back();
                pendingFreeStagingBuffers.pop_back();
            }
        }

        if (bFreeToOS)
        {
            int32 NumFreeBuffers = bImmediately ? freeStagingBuffers.size() : NumOriginalFreeBuffers;
            for (int32 Index = NumFreeBuffers - 1; Index >= 0; --Index)
            {
                FreeEntry &Entry = freeStagingBuffers[Index];
                if (bImmediately || Entry.FrameNumber + NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS < GFrameNumberRenderThread)
                {
                    usedMemory -= Entry.StagingBuffer->GetSize();
                    Entry.StagingBuffer->Destroy();
                    delete Entry.StagingBuffer;

                    freeStagingBuffers[Index] = freeStagingBuffers.back();
                    freeStagingBuffers.pop_back();
                }
            }
        }
    }

    void StagingManager::ProcessPendingFree(bool bImmediately, bool bFreeToOS)
    {
        /* FScopeLock Lock(&StagingLock); */
        ProcessPendingFreeNoLock(bImmediately, bFreeToOS);
    }
}