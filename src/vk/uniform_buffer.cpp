#include "resources.h"
#include "device.h"
#include "context.h"
#include "command_buffer.h"
#include "rhi.h"

#include "render_core/shader_parameter_struct.h"
#include "RHI/RHICommandList.h"

enum
{
    PackedUniformsRingBufferSize = 16 * 1024 * 1024
};

static void UpdateUniformBufferConstants(Device *device, void *DestinationData, const void *SourceData, const UniformBufferLayout *Layout)
{
    const bool bSupportsBindless = device->SupportsBindless();
    if (bSupportsBindless)
    {
        printf("Don't support Bindless %s %d\n", __FILE__, __LINE__);
        exit(-1);
    }

    check(DestinationData != nullptr);
    check(SourceData != nullptr);

    // First copy wholesale
    memcpy(DestinationData, SourceData, Layout->ConstantBufferSize);
}

static bool UseRingBuffer(UniformBufferUsage Usage)
{
    // Add a cvar to control this behavior?
    return (Usage == UniformBuffer_SingleDraw || Usage == UniformBuffer_SingleFrame);
}

static void UpdateUniformBufferHelper(CommandListContext &Context, VulkanUniformBuffer *VulkanUniformBuffer,
                                      const void *Data, bool bUpdateConstants = true)
{
    CmdBuffer *CmdBuffer = Context.GetCommandBufferManager()->GetActiveCmdBufferDirect();

    Device *device = Context.GetDevice();
    const int32 DataSize = VulkanUniformBuffer->GetLayout().ConstantBufferSize;

    auto CopyUniformBufferData = [&](void *DestinationData, const void *SourceData)
    {
        if (bUpdateConstants)
        {
            // Update constants as the data is copied
            UpdateUniformBufferConstants(device, DestinationData, SourceData, VulkanUniformBuffer->GetLayoutPtr());
        }
        else
        {
            // Don't touch constant, copy the data as-is
            memcpy(DestinationData, SourceData, DataSize);
        }
    };

    if (UseRingBuffer(VulkanUniformBuffer->Usage))
    {
        printf("Don't support  %s %d\n", __FILE__, __LINE__);
        exit(-1);
        VulkanUniformBufferUploader *UniformBufferUploader = Context.GetUniformBufferUploader();
        const VkDeviceSize UBOffsetAlignment = device->GetLimits().minUniformBufferOffsetAlignment;

        const VmaAllocation &RingBufferAllocation = UniformBufferUploader->GetCPUBufferAllocation();
        uint64 RingBufferOffset = UniformBufferUploader->AllocateMemory(DataSize, UBOffsetAlignment, CmdBuffer);

        VulkanUniformBuffer->allocation = RingBufferAllocation;
        VulkanUniformBuffer->handle = UniformBufferUploader->GetCPUBufferHandle();
        VulkanUniformBuffer->info = UniformBufferUploader->GetCPUBufferAllocationInfo();
        VulkanUniformBuffer->offset = RingBufferOffset;

        uint8 *UploadLocation = UniformBufferUploader->GetCPUMappedPointer() + RingBufferOffset;
        CopyUniformBufferData(UploadLocation, Data);
    }
    else
    {
        check(CmdBuffer->IsOutsideRenderPass());

        VulkanRHI::TempFrameAllocationBuffer::TempAllocInfo LockInfo;
        Context.GetTempFrameAllocationBuffer().Alloc(DataSize, 16, LockInfo);
        CopyUniformBufferData(LockInfo.Data, Data);

        VkBufferCopy Region;
        Region.size = DataSize;
        Region.srcOffset = LockInfo.CurrentOffset;
        Region.dstOffset = VulkanUniformBuffer->GetOffset();
        VkBuffer UBBuffer = VulkanUniformBuffer->handle;
        VkBuffer LockHandle = LockInfo.buffer;

        printf("没同步？？\n");
        vkCmdCopyBuffer(CmdBuffer->GetHandle(), LockHandle, UBBuffer, 1, &Region);
    }
}

VulkanUniformBuffer::VulkanUniformBuffer(Device &InDevice, std::shared_ptr<const UniformBufferLayout> InLayout, const void *Contents, UniformBufferUsage InUsage, UniformBufferValidation Validation)
    : UniformBuffer(InLayout), device(&InDevice), Usage(InUsage)
{
    // Verify the correctness of our thought pattern how the resources are delivered
    //	- If we have at least one resource, we also expect ResourceOffset to have an offset
    //	- Meaning, there is always a uniform buffer with a size specified larged than 0 bytes
    check(InLayout->Resources.size() > 0 || InLayout->ConstantBufferSize > 0);

    // Setup resource table
    const uint32 NumResources = InLayout->Resources.size();
    if (NumResources > 0)
    {
        // Transfer the resource table to an internal resource-array
        ResourceTable.clear();
        ResourceTable.resize(NumResources);

        if (Contents)
        {
            for (uint32 Index = 0; Index < NumResources; ++Index)
            {
                ResourceTable[Index] = GetShaderParameterResourceRHI(Contents, InLayout->Resources[Index].memberOffset, InLayout->Resources[Index].memberType);
            }
        }
    }

    if (InLayout->ConstantBufferSize > 0)
    {
        if (UseRingBuffer(InUsage))
        {
            if (Contents)
            {
                VulkanUniformBuffer *UniformBuffer = this;

                CommandListContextImmediate &Context = device->GetImmediateContext();
                UpdateUniformBufferHelper(Context, UniformBuffer, Contents);
            }
        }
        else
        {
            // Set it directly as there is no previous one
            VkBufferCreateInfo bufferCI{};
            bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCI.size = InLayout->ConstantBufferSize;
            bufferCI.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            VmaAllocationCreateInfo allocationCI{};
            allocationCI.usage = VMA_MEMORY_USAGE_AUTO;
            allocationCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VkResult result = vmaCreateBuffer(device->GetAllocator(), &bufferCI, &allocationCI, &handle, &allocation, &info);
#ifdef DEBUG_BUFFER_CREATE_DESTROY
            std::cout << "Create VulkanUniformBuffer: " << std::hex << handle << std::endl;
#endif
            offset = 0;

            if (Contents)
            {
                UpdateUniformBufferConstants(device, info.pMappedData, Contents, InLayout.get());
                vmaFlushAllocation(device->GetAllocator(), allocation, 0, VK_WHOLE_SIZE);
            }
        }
    }
}

VulkanUniformBuffer::~VulkanUniformBuffer()
{
    vmaDestroyBuffer(device->GetAllocator(), handle, allocation);
#ifdef DEBUG_BUFFER_CREATE_DESTROY
    std::cout << "Destroy VulkanUniformBuffer: " << std::hex << handle << std::endl;
#endif
}

void VulkanUniformBuffer::UpdateResourceTable(const UniformBufferLayout &InLayout, const void *Contents, int32 NumResources)
{
    check(ResourceTable.size() == NumResources);

    for (int32 Index = 0; Index < NumResources; ++Index)
    {
        const auto Parameter = InLayout.Resources[Index];
        ResourceTable[Index] = GetShaderParameterResourceRHI(Contents, Parameter.memberOffset, Parameter.memberType);
    }
}

void VulkanUniformBuffer::UpdateResourceTable(RHIResource **Resources, int32 ResourceNum)
{
    check(ResourceTable.size() == ResourceNum);

    for (int32 ResourceIndex = 0; ResourceIndex < ResourceNum; ++ResourceIndex)
    {
        ResourceTable[ResourceIndex] = Resources[ResourceIndex];
    }
}

std::shared_ptr<UniformBuffer> RHI::CreateUniformBuffer(const void *Contents, std::shared_ptr<const UniformBufferLayout> Layout, UniformBufferUsage Usage, UniformBufferValidation Validation)
{
    return std::make_shared<VulkanUniformBuffer>(*device, Layout, Contents, Usage, Validation);
}

inline void RHI::UpdateUniformBuffer(RHICommandListBase &RHICmdList, VulkanUniformBuffer *UniformBuffer, const void *Contents)
{
    const UniformBufferLayout &Layout = UniformBuffer->GetLayout();

    const int32 ConstantBufferSize = Layout.ConstantBufferSize;
    const int32 NumResources = Layout.Resources.size();

    bool bUseUpload = !RHICmdList.IsInsideRenderPass();
    const bool bUseRingBuffer = UseRingBuffer(UniformBuffer->Usage);

    if (!bUseUpload && !bUseRingBuffer)
    {
        check(0);
    }

    bool bRHIBypass = RHICmdList.Bypass();

    if (bRHIBypass)
    {
        if (ConstantBufferSize > 0)
        {
            if (bUseUpload || bUseRingBuffer)
            {
                CommandListContext &Context = device->GetImmediateContext();
                UpdateUniformBufferHelper(Context, UniformBuffer, Contents);
            }
            else
            {
                check(0);
            }
        }

        UniformBuffer->UpdateResourceTable(Layout, Contents, NumResources);
    }
    else
    {
        check(0);
    }
}

void RHI::UpdateUniformBuffer(RHICommandListBase &RHICmdList, UniformBuffer *UniformBufferRHI, const void *Contents)
{
    VulkanUniformBuffer *uniformBuffer = static_cast<VulkanUniformBuffer *>(UniformBufferRHI);
    UpdateUniformBuffer(RHICmdList, uniformBuffer, Contents);
}

VulkanUniformBufferUploader::VulkanUniformBufferUploader(Device *InDevice)
    : VulkanRHI::DeviceChild(InDevice), CPUBuffer(nullptr)
{
    if (device->HasUnifiedMemory())
    {
        CPUBuffer = new VulkanRingBuffer(InDevice, PackedUniformsRingBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
    else
    {
        CPUBuffer = new VulkanRingBuffer(InDevice, PackedUniformsRingBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
}

VulkanUniformBufferUploader::~VulkanUniformBufferUploader() { delete CPUBuffer; }

VulkanRingBuffer::VulkanRingBuffer(Device *InDevice, uint64 TotalSize, VkFlags Usage, VkMemoryPropertyFlags MemPropertyFlags)
    : VulkanRHI::DeviceChild(InDevice), BufferSize(TotalSize), BufferOffset(0), BufferAddress(0), MinAlignment(0)
{
    check(TotalSize <= (uint64)UINT32_MAX);

    VkBufferCreateInfo bufferCI{};
    bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCI.size = BufferSize;
    bufferCI.usage = Usage;
    VmaAllocationCreateInfo allcationCI{};
    allcationCI.usage = VMA_MEMORY_USAGE_AUTO;
    allcationCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkResult result = vmaCreateBuffer(device->GetAllocator(), &bufferCI, &allcationCI, &handle, &allocation, &info);
#ifdef DEBUG_BUFFER_CREATE_DESTROY
    std::cout << "Create VulkanRingBuffer: " << std::hex << handle << std::endl;
#endif

    printf("Notice Min Alignment of Ring Buffer %s %d\n", __FILE__, __LINE__);
    MinAlignment = 0;

    // Start by wrapping around to set up the correct fence
    BufferOffset = TotalSize;

    if (InDevice->GetOptionalExtensions().HasBufferDeviceAddress)
    {
        printf("Don't support BufferDeviceAddress %s %d\n", __FILE__, __LINE__);
        exit(-1);
    }
}

VulkanRingBuffer::~VulkanRingBuffer()
{
    vmaDestroyBuffer(device->GetAllocator(), handle, allocation);
#ifdef DEBUG_BUFFER_CREATE_DESTROY
    std::cout << "Destroy VulkanRingBuffer: " << std::hex << handle << std::endl;
#endif
    handle = VK_NULL_HANDLE;
    allocation = VK_NULL_HANDLE;
}

uint64 VulkanRingBuffer::WrapAroundAllocateMemory(uint64 Size, uint32 Alignment, CmdBuffer *InCmdBuffer)
{
    uint64 AllocationOffset = Align<uint64>(BufferOffset, Alignment);
    ensure(AllocationOffset + Size > BufferSize);

    // Check to see if we can wrap around the ring buffer
    if (FenceCmdBuffer)
    {
        if (FenceCounter == FenceCmdBuffer->GetFenceSignaledCounter())
        {
        }
    }

    BufferOffset = Size;

    FenceCmdBuffer = InCmdBuffer;
    FenceCounter = InCmdBuffer->GetSubmittedFenceCounter();

    return 0;
}
