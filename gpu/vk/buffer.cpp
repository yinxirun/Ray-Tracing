#include <memory>
#include <unordered_map>
#include "gpu/RHI/RHIResources.h"
#include "gpu/RHI/RHICommandList.h"
#include "rhi.h"
#include "resources.h"
#include "context.h"

// When nonzero, non-volatile buffer locks will always use staging buffers. Useful for debugging.
// default: 0"
int32 GVulkanForceStagingBufferOnLock = 0;

static std::unordered_map<VulkanMultiBuffer *, VulkanRHI::PendingBufferLock> GPendingLockIBs;

static VulkanRHI::PendingBufferLock GetPendingBufferLock(VulkanMultiBuffer *Buffer)
{
    VulkanRHI::PendingBufferLock PendingLock;

    // Found only if it was created for Write
    // FScopeLock ScopeLock(&GPendingLockIBsMutex);
    bool bFound = GPendingLockIBs.count(Buffer) > 0;
    check(bFound);
    PendingLock = GPendingLockIBs[Buffer];
    GPendingLockIBs.erase(Buffer);
    return PendingLock;
}

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

void *VulkanMultiBuffer::Lock(RHICommandListBase &RHICmdList, ResourceLockMode LockMode, uint32 LockSize, uint32 Offset)
{
    // Use the immediate context for write operations, since we are only accessing allocators.
    CommandListContext &Context =
        CommandListContext::GetVulkanContext(LockMode == RLM_WriteOnly ? *(rhi->GetDefaultContext()) : RHICmdList.GetContext());

    return Lock(Context, LockMode, LockSize, Offset);
}
void *VulkanMultiBuffer::Lock(CommandListContext &Context, ResourceLockMode LockMode, uint32 LockSize, uint32 Offset)
{
    void *Data = nullptr;
    uint32 DataOffset = 0;

    const bool bVolatile = EnumHasAnyFlags(GetUsage(), BUF_Volatile);
    check(lockStatus == LockStatus::Unlocked);

    lockStatus = LockStatus::Locked;
    ++LockCounter;

    if (bVolatile)
    {
        if (LockMode == RLM_ReadOnly)
        {
            check(0);
            // checkf(0, TEXT("Volatile buffers can't be locked for read."));
        }
        else
        {
            printf("ERROR: Don't support Volatile Buffer %s %d\n", __FILE__, __LINE__);
            exit(-1);
            // BufferAlloc &bufferAlloc = BufferAllocs[0];

            // FTempFrameAllocationBuffer::FTempAllocInfo VolatileAlloc;
            // Context.GetTempFrameAllocationBuffer().Alloc(LockSize + Offset, 256, VolatileAlloc);
            // check(!VolatileAlloc.Allocation.HasAllocation());

            // BufferAlloc.Alloc.Reference(VolatileAlloc.Allocation);
            // Data = BufferAlloc.HostPtr = VolatileAlloc.Data;
            // DataOffset = Offset;

            // // Patch our alloc to go directly to our offset
            // BufferAlloc.Alloc.Offset += VolatileAlloc.CurrentOffset;
            // BufferAlloc.Alloc.Size = VolatileAlloc.Size;

            // BufferAlloc.DeviceAddress = GetBufferDeviceAddress(Device, BufferAlloc.Alloc.GetBufferHandle()) + BufferAlloc.Alloc.Offset;
        }
    }
    else
    {
        const bool bDynamic = EnumHasAnyFlags(GetUsage(), BUF_Dynamic);
        const bool bStatic = EnumHasAnyFlags(GetUsage(), BUF_Static) || !(bVolatile || bDynamic);
        const bool bUAV = EnumHasAnyFlags(GetUsage(), BUF_UnorderedAccess);
        const bool bSR = EnumHasAnyFlags(GetUsage(), BUF_ShaderResource);
        const bool bUnifiedMem = device->HasUnifiedMemory();

        check(bStatic || bDynamic || bUAV || bSR);

        if (LockMode == RLM_ReadOnly)
        {
            check(IsInRenderingThread() && Context.IsImmediate());

            if (bUnifiedMem)
            {
                Data = BufferAllocs[CurrentBufferIndex].HostPtr;
                DataOffset = Offset;
                lockStatus = LockStatus::PersistentMapping;
            }
            else
            {
                device->PrepareForCPURead();
                CmdBuffer *CmdBuffer = Context.GetCommandBufferManager()->GetUploadCmdBuffer();

                // Make sure any previous tasks have finished on the source buffer.
                VkMemoryBarrier BarrierBefore = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT};
                vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &BarrierBefore, 0, nullptr, 0, nullptr);

                // Create a staging buffer we can use to copy data from device to cpu.
                VulkanRHI::StagingBuffer *StagingBuffer = device->GetStagingManager().AcquireBuffer(LockSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

                // Fill the staging buffer with the data on the device.
                VkBufferCopy Regions;
                Regions.size = LockSize;
                Regions.srcOffset = Offset + BufferAllocs[CurrentBufferIndex].allocInfo.offset;
                Regions.dstOffset = 0;

                vkCmdCopyBuffer(CmdBuffer->GetHandle(), BufferAllocs[CurrentBufferIndex].handle, StagingBuffer->GetHandle(), 1, &Regions);

                // Setup barrier.
                VkMemoryBarrier BarrierAfter = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
                vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &BarrierAfter, 0, nullptr, 0, nullptr);

                // Force upload.
                Context.GetCommandBufferManager()->SubmitUploadCmdBuffer();
                device->WaitUntilIdle();

                // Flush.
                StagingBuffer->FlushMappedMemory();

                // Get mapped pointer.
                Data = StagingBuffer->GetMappedPointer();

                // Release temp staging buffer during unlock.
                VulkanRHI::PendingBufferLock PendingLock;
                PendingLock.Offset = 0;
                PendingLock.Size = LockSize;
                PendingLock.LockMode = LockMode;
                PendingLock.stagingBuffer = StagingBuffer;

                {
                    // FScopeLock ScopeLock(&GPendingLockIBsMutex);
                    check(!GPendingLockIBs.count(this));
                    GPendingLockIBs.insert(std::pair(this, PendingLock));
                }

                Context.GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();
            }
        }
        else
        {
            check(LockMode == RLM_WriteOnly);

            // Always use staging buffers to update 'Static' buffers since they maybe be in use by GPU atm
            const bool bUseStagingBuffer = (bStatic || !bUnifiedMem) || GVulkanForceStagingBufferOnLock;
            if (bUseStagingBuffer)
            {
                // NOTE: No need to change the CurrentBufferIndex if we're using a staging buffer for the copy

                VulkanRHI::PendingBufferLock PendingLock;
                PendingLock.Offset = Offset;
                PendingLock.Size = LockSize;
                PendingLock.LockMode = LockMode;

                VulkanRHI::StagingBuffer *StagingBuffer = device->GetStagingManager().AcquireBuffer(LockSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                PendingLock.stagingBuffer = StagingBuffer;
                Data = StagingBuffer->GetMappedPointer();

                {
                    // FScopeLock ScopeLock(&GPendingLockIBsMutex);
                    check(!GPendingLockIBs.count(this));
                    GPendingLockIBs.insert(std::pair(this, PendingLock));
                }
            }
            else
            {
                AdvanceBufferIndex();

                Data = BufferAllocs[CurrentBufferIndex].HostPtr;
                DataOffset = Offset;
                lockStatus = LockStatus::PersistentMapping;
            }
        }
    }

    check(Data);
    return (uint8 *)Data + DataOffset;
}

void VulkanMultiBuffer::ReleaseOwnership()
{
    check(lockStatus == LockStatus::Unlocked);

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
            CurrentBufferAlloc.allocStatus = BufferAlloc::AllocStatus::NeedsFence;
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
            if ((BufferAlloc.allocStatus == BufferAlloc::AllocStatus::Pending) && BufferAlloc.Fence->Poll())
            {
                BufferAlloc.allocStatus = BufferAlloc::AllocStatus::Available;
                BufferAlloc.Fence->Clear();
            }

            if (BufferAlloc.allocStatus == BufferAlloc::AllocStatus::Available)
            {
                FreePreviousAlloc();

                CurrentBufferIndex = BufferIndex;
                BufferAllocs[CurrentBufferIndex].allocStatus = BufferAlloc::AllocStatus::InUse;
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
        allocCi.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocCi.flags = bUnifiedMem ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0;
        if (VK_SUCCESS != vmaCreateBuffer(device->GetAllocator(), &ci, &allocCi,
                                          &NewBufferAlloc.handle, &NewBufferAlloc.alloc, &NewBufferAlloc.allocInfo))
        {
            throw std::runtime_error("Fail to allocate buffer");
        }

        void *hostPtr = nullptr;
        if (bUnifiedMem)
            vmaMapMemory(device->GetAllocator(), NewBufferAlloc.alloc, &hostPtr);

        NewBufferAlloc.HostPtr = hostPtr;
        NewBufferAlloc.Fence = new VulkanGPUFence();
        NewBufferAlloc.allocStatus = BufferAlloc::AllocStatus::InUse;
        NewBufferAlloc.DeviceAddress = GetBufferDeviceAddress(device, NewBufferAlloc.handle) + NewBufferAlloc.allocInfo.offset;
    }
}

void VulkanMultiBuffer::UpdateBufferAllocStates(CommandListContext &Context)
{
    for (int32 BufferIndex = 0; BufferIndex < BufferAllocs.size(); ++BufferIndex)
    {
        if (CurrentBufferIndex == BufferIndex)
        {
            continue;
        }

        BufferAlloc &bufferAlloc = BufferAllocs[BufferIndex];
        if (bufferAlloc.allocStatus == BufferAlloc::AllocStatus::Pending)
        {
            if (bufferAlloc.Fence->Poll())
            {
                bufferAlloc.allocStatus = BufferAlloc::AllocStatus::Available;
                bufferAlloc.Fence->Clear();
            }
        }
        else if (bufferAlloc.allocStatus == BufferAlloc::AllocStatus::NeedsFence)
        {
            Context.WriteGPUFence(bufferAlloc.Fence);
            bufferAlloc.allocStatus = BufferAlloc::AllocStatus::Pending;
        }
    }
}

void VulkanMultiBuffer::Unlock(RHICommandListBase *RHICmdList, CommandListContext *Context)
{
    check(RHICmdList || Context);

    const bool bDynamic = EnumHasAnyFlags(GetUsage(), BUF_Dynamic);
    const bool bVolatile = EnumHasAnyFlags(GetUsage(), BUF_Volatile);
    const bool bStatic = EnumHasAnyFlags(GetUsage(), BUF_Static) || !(bVolatile || bDynamic);
    const bool bSR = EnumHasAnyFlags(GetUsage(), BUF_ShaderResource);

    check(lockStatus != LockStatus::Unlocked);

    if (bVolatile)
    {
        printf("Don't support Volatile Buffer %s %d\n", __FILE__, __LINE__);
        exit(-1);
        // if (RHICmdList && RHICmdList->IsTopOfPipe())
        // {
        // 	RHICmdList->EnqueueLambda([this](FRHICommandListBase&)
        // 	{
        // 		UpdateLinkedViews();
        // 	});
        // }
        // else
        // {
        // 	UpdateLinkedViews();
        // }
    }
    else if (lockStatus == LockStatus::PersistentMapping)
    {
        printf("ERROR: Don't support PersistentMapping Buffer %s %d\n", __FILE__, __LINE__);
        exit(-1);
        // if (Context)
        // {
        //     UpdateBufferAllocStates(*Context);
        //     UpdateLinkedViews();
        // }
        // else
        // {
        //     RHICmdList->EnqueueLambda([this](FRHICommandListBase &CmdList)
        //                               {
        //     	FVulkanCommandListContext& Context = FVulkanCommandListContext::GetVulkanContext(CmdList.GetContext());
        //     	UpdateBufferAllocStates(Context);
        //     	UpdateLinkedViews(); });
        // }
    }
    else
    {
        check(bStatic || bDynamic || bSR);

        VulkanRHI::PendingBufferLock PendingLock = GetPendingBufferLock(this);

        PendingLock.stagingBuffer->FlushMappedMemory();

        if (PendingLock.LockMode == RLM_ReadOnly)
        {
            // Just remove the staging buffer here.
            device->GetStagingManager().ReleaseBuffer(0, PendingLock.stagingBuffer);
        }
        else if (PendingLock.LockMode == RLM_WriteOnly)
        {
            if (Context /*|| (RHICmdList && RHICmdList->IsBottomOfPipe())*/)
            {
                /*if (!Context)
                {
                    Context = &CommandListContext::GetVulkanContext(RHICmdList->GetContext());
                }*/

                VulkanMultiBuffer::InternalUnlock(*Context, PendingLock, this, CurrentBufferIndex);
            }
            else
            {
                printf("ERROR: Don't support CommandList %s %d\n", __FILE__, __LINE__);
                // ALLOC_COMMAND_CL(*RHICmdList, FRHICommandMultiBufferUnlock)(device, PendingLock, this, CurrentBufferIndex);
            }
        }
    }

    lockStatus = LockStatus::Unlocked;
}

void VulkanMultiBuffer::InternalUnlock(CommandListContext &Context, VulkanRHI::PendingBufferLock &PendingLock,
                                       VulkanMultiBuffer *MultiBuffer, int32 InBufferIndex)
{
    const uint32 LockSize = PendingLock.Size;
    const uint32 LockOffset = PendingLock.Offset;
    VulkanRHI::StagingBuffer *stagingBuffer = PendingLock.stagingBuffer;
    PendingLock.stagingBuffer = nullptr;

    // We need to do this on the active command buffer instead of using an upload command buffer. The high level code sometimes reuses the same
    // buffer in sequences of upload / dispatch, upload / dispatch, so we need to order the copy commands correctly with respect to the dispatches.
    CmdBuffer *Cmd = Context.GetCommandBufferManager()->GetActiveCmdBuffer();
    check(Cmd && Cmd->IsOutsideRenderPass());
    VkCommandBuffer CmdBuffer = Cmd->GetHandle();

    /*VulkanRHI::DebugHeavyWeightBarrier(CmdBuffer, 16);*/

    VkBufferCopy Region{};
    Region.size = LockSize;
    // Region.srcOffset = 0;
    Region.dstOffset = LockOffset + MultiBuffer->BufferAllocs[InBufferIndex].offset;
    vkCmdCopyBuffer(CmdBuffer, stagingBuffer->GetHandle(), MultiBuffer->BufferAllocs[InBufferIndex].handle, 1, &Region);

    // High level code expects the data in MultiBuffer to be ready to read
    VkMemoryBarrier BarrierAfter = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
    vkCmdPipelineBarrier(CmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &BarrierAfter, 0, nullptr, 0, nullptr);

    MultiBuffer->GetParent()->GetStagingManager().ReleaseBuffer(Cmd, stagingBuffer);

    MultiBuffer->UpdateBufferAllocStates(Context);
    MultiBuffer->UpdateLinkedViews();
}

std::shared_ptr<Buffer> RHI::CreateBuffer(BufferDesc const &Desc, Access ResourceState, ResourceCreateInfo &CreateInfo)
{
    return std::make_shared<VulkanMultiBuffer>(device, Desc, CreateInfo, nullptr);
}