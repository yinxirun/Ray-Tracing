#include <memory>
#include "gpu/RHI/RHIResources.h"
#include "rhi.h"
#include "resources.h"

VulkanMultiBuffer::VulkanMultiBuffer(Device *InDevice, BufferDesc const &InBufferDesc,
                         ResourceCreateInfo &CreateInfo, RHICommandListBase *InRHICmdList,
                         const RHITransientHeapAllocation *InTransientHeapAllocation)
    : Buffer(InBufferDesc), VulkanRHI::DeviceChild(InDevice)
{
    const bool bZeroSize = (InBufferDesc.Size == 0);
    usageFlags = UEToVKBufferUsageFlags(InDevice, InBufferDesc.Usage, bZeroSize);

    if (!bZeroSize)
    {
        check(InDevice);

        const bool bVolatile = EnumHasAnyFlags(InBufferDesc.Usage, BUF_Volatile);
        if (bVolatile)
        {
            check(InRHICmdList);
            printf("ERROR: Don't support Volatile Buffer %s %d\n", __FILE__, __LINE__);
            // // Volatile buffers always work out of the same first slot
            // CurrentBufferIndex = BufferAllocs.size() ;
            // BufferAllocs.push_back({});

            // // Get a dummy buffer as sometimes the high-level misbehaves and tries to use SRVs off volatile buffers before filling them in...
            // void *Data = Lock(*InRHICmdList, RLM_WriteOnly, InBufferDesc.Size, 0);

            // if (CreateInfo.ResourceArray)
            // {
            //     uint32 CopyDataSize = FMath::Min(InBufferDesc.Size, CreateInfo.ResourceArray->GetResourceDataSize());
            //     FMemory::Memcpy(Data, CreateInfo.ResourceArray->GetResourceData(), CopyDataSize);
            // }
            // else
            // {
            //     FMemory::Memzero(Data, InBufferDesc.Size);
            // }

            // Unlock(*InRHICmdList);
        }
        else
        {
            const bool bUnifiedMem = InDevice->HasUnifiedMemory();

            if (InTransientHeapAllocation != nullptr)
            {
                printf("ERROR: Don't support Transient Heap Allocation %s %d\n", __FILE__, __LINE__);
                exit(-1);
                // FBufferAlloc NewBufferAlloc;
                // NewBufferAlloc.Alloc = FVulkanTransientHeap::GetVulkanAllocation(*InTransientHeapAllocation);
                // NewBufferAlloc.HostPtr = bUnifiedMem ? NewBufferAlloc.Alloc.GetMappedPointer(Device) : nullptr;
                // NewBufferAlloc.DeviceAddress = GetBufferDeviceAddress(InDevice, NewBufferAlloc.Alloc.GetBufferHandle()) + NewBufferAlloc.Alloc.Offset;
                // check(NewBufferAlloc.Alloc.Offset % BufferAlignment == 0);
                // check(NewBufferAlloc.Alloc.Size >= InBufferDesc.Size);
                // CurrentBufferIndex = BufferAllocs.Add(NewBufferAlloc);
            }
            else
            {
                AdvanceBufferIndex();
            }

            if (CreateInfo.ResourceArray)
            {
                printf("ERROR: Don't support CreateInfo.ResourceArray %s %d\n", __FILE__, __LINE__);
                exit(-1);
                // check(InRHICmdList);

                // uint32 CopyDataSize = std::min(InBufferDesc.Size, CreateInfo.ResourceArray->GetResourceDataSize());
                // // We know this buffer is not in use by GPU atm. If we do have a direct access initialize it without extra copies
                // if (bUnifiedMem)
                // {
                //     FMemory::Memcpy(BufferAllocs[CurrentBufferIndex].HostPtr, CreateInfo.ResourceArray->GetResourceData(), CopyDataSize);
                // }
                // else
                // {
                //     void *Data = Lock(*InRHICmdList, RLM_WriteOnly, CopyDataSize, 0);
                //     FMemory::Memcpy(Data, CreateInfo.ResourceArray->GetResourceData(), CopyDataSize);
                //     Unlock(*InRHICmdList);
                // }

                // CreateInfo.ResourceArray->Discard();
            }
        }
    }
}

VulkanMultiBuffer::~VulkanMultiBuffer()
{
    ReleaseOwnership();
}

void VulkanMultiBuffer::ReleaseOwnership()
{
    check(LockStatus == ELockStatus::Unlocked);

    Buffer::ReleaseOwnership();

    uint64 TotalSize = 0;
    for (int32 Index = 0; Index < BufferAllocs.size(); ++Index)
    {
        if (BufferAllocs[Index].alloc != VK_NULL_HANDLE)
        {
            TotalSize += BufferAllocs[Index].allocInfo.size;
            vmaDestroyBuffer(device->GetAllocator(), BufferAllocs[Index].handle, BufferAllocs[Index].alloc);
        }
        BufferAllocs[Index].Fence = nullptr;
    }
    BufferAllocs.clear();

    if (TotalSize > 0)
    {
        // UpdateVulkanBufferStats(GetDesc(), TotalSize, false);
    }
}

static VkDeviceAddress GetBufferDeviceAddress(Device *device, VkBuffer Buffer)
{
    if (/*device->GetOptionalExtensions().HasBufferDeviceAddress*/ false)
    {
        VkBufferDeviceAddressInfoKHR DeviceAddressInfo;
        ZeroVulkanStruct(DeviceAddressInfo, VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);
        DeviceAddressInfo.buffer = Buffer;
        return vkGetBufferDeviceAddressKHR(device->GetInstanceHandle(), &DeviceAddressInfo);
    }
    return 0;
}

VkBufferUsageFlags VulkanMultiBuffer::UEToVKBufferUsageFlags(Device *InDevice, BufferUsageFlags InUEUsage, bool bZeroSize)
{
    // Always include TRANSFER_SRC since hardware vendors confirmed it wouldn't have any performance cost and we need it for some debug functionalities.
    VkBufferUsageFlags OutVkUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    auto TranslateFlag = [&OutVkUsage, &InUEUsage](BufferUsageFlags SearchUEFlag, VkBufferUsageFlags AddedIfFound, VkBufferUsageFlags AddedIfNotFound = 0)
    {
        const bool HasFlag = EnumHasAnyFlags(InUEUsage, SearchUEFlag);
        OutVkUsage |= HasFlag ? AddedIfFound : AddedIfNotFound;
    };

    TranslateFlag(BUF_VertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    TranslateFlag(BUF_IndexBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    TranslateFlag(BUF_StructuredBuffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    TranslateFlag(BUF_UniformBuffer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

#if VULKAN_RHI_RAYTRACING
    TranslateFlag(BUF_AccelerationStructure, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
#endif

    if (!bZeroSize)
    {
        TranslateFlag(BUF_UnorderedAccess, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
        TranslateFlag(BUF_DrawIndirect, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        TranslateFlag(BUF_KeepCPUAccessible, (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));
        TranslateFlag(BUF_ShaderResource, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);

        TranslateFlag(BUF_Volatile, 0, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

#if VULKAN_RHI_RAYTRACING
        if (InDevice->GetOptionalExtensions().HasRaytracingExtensions())
        {
            OutVkUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

            TranslateFlag(BUF_AccelerationStructure, 0, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
        }
#endif
        // For descriptors buffers
        if (false /*InDevice->GetOptionalExtensions().HasBufferDeviceAddress*/)
        {
            OutVkUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        }
    }

    return OutVkUsage;
}

void VulkanMultiBuffer::AdvanceBufferIndex()
{
    auto FreePreviousAlloc = [&]()
    {
        if (CurrentBufferIndex >= 0)
        {
            BufferAlloc &CurrentBufferAlloc = BufferAllocs[CurrentBufferIndex];
            CurrentBufferAlloc.AllocStatus = BufferAlloc::EAllocStatus::NeedsFence;
            CurrentBufferAlloc.Fence->Clear();
        }
    };

    // Try to see if one of the buffers in our pool can be reused
    if (BufferAllocs.size() > 1)
    {
        for (int32 BufferIndex = 0; BufferIndex < BufferAllocs.size(); ++BufferIndex)
        {
            if (CurrentBufferIndex == BufferIndex)
                continue;

            // Fences are only written on Unlock(), but are polled on lock/unlock
            BufferAlloc &BufferAlloc = BufferAllocs[BufferIndex];
            if ((BufferAlloc.AllocStatus == BufferAlloc::EAllocStatus::Pending) && BufferAlloc.Fence->Poll())
            {
                BufferAlloc.AllocStatus = BufferAlloc::EAllocStatus::Available;
                BufferAlloc.Fence->Clear();
            }

            if (BufferAlloc.AllocStatus == BufferAlloc::EAllocStatus::Available)
            {
                FreePreviousAlloc();

                CurrentBufferIndex = BufferIndex;
                BufferAllocs[CurrentBufferIndex].AllocStatus = BufferAlloc::EAllocStatus::InUse;
                return;
            }
        }
    }

    // Allocate a new buffer
    {
        FreePreviousAlloc();

        const bool bUnifiedMem = device->HasUnifiedMemory();

        CurrentBufferIndex = BufferAllocs.size();
        BufferAllocs.push_back({});
        BufferAlloc &NewBufferAlloc = BufferAllocs[CurrentBufferIndex];

        VkBufferCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ci.size = GetSize();
        ci.usage = UEToVKBufferUsageFlags(device, GetUsage(), ci.size == 0);
        VmaAllocationCreateInfo allocCi{};
        allocCi.usage = VMA_MEMORY_USAGE_AUTO;
        if (VK_SUCCESS != vmaCreateBuffer(device->GetAllocator(), &ci, &allocCi,
                                          &NewBufferAlloc.handle, &NewBufferAlloc.alloc, &NewBufferAlloc.allocInfo))
        {
            throw std::runtime_error("Fail to allocate buffer");
        }

        NewBufferAlloc.HostPtr = nullptr;
        NewBufferAlloc.Fence = new VulkanGPUFence();
        NewBufferAlloc.AllocStatus = BufferAlloc::EAllocStatus::InUse;
        NewBufferAlloc.DeviceAddress = GetBufferDeviceAddress(device, NewBufferAlloc.handle) + NewBufferAlloc.allocInfo.offset;
    }
}

std::shared_ptr<Buffer> RHI::CreateBuffer(BufferDesc const &Desc, Access ResourceState, ResourceCreateInfo &CreateInfo)
{
    return std::make_shared<VulkanMultiBuffer>(device, Desc, CreateInfo, nullptr);
}