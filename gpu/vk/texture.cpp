#include "resources.h"
#include "device.h"
#include "private.h"
#include "command_buffer.h"
#include "context.h"
#include "vulkan_memory.h"
#include "rhi.h"
#include "gpu/RHI/RHIResources.h"
#include "gpu/RHI/RHICommandList.h"
#include "gpu/core/pixel_format.h"
#include <unordered_map>

static const VkImageTiling GVulkanViewTypeTilingMode[7] =
    {
        VK_IMAGE_TILING_LINEAR,  // VK_IMAGE_VIEW_TYPE_1D
        VK_IMAGE_TILING_OPTIMAL, // VK_IMAGE_VIEW_TYPE_2D
        VK_IMAGE_TILING_OPTIMAL, // VK_IMAGE_VIEW_TYPE_3D
        VK_IMAGE_TILING_OPTIMAL, // VK_IMAGE_VIEW_TYPE_CUBE
        VK_IMAGE_TILING_LINEAR,  // VK_IMAGE_VIEW_TYPE_1D_ARRAY
        VK_IMAGE_TILING_OPTIMAL, // VK_IMAGE_VIEW_TYPE_2D_ARRAY
        VK_IMAGE_TILING_OPTIMAL, // VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
};

int32 GVulkanDepthStencilForceStorageBit = 0;

struct TextureLock
{
    RHIResource *Texture;
    uint32 MipIndex;
    uint32 LayerIndex;
    TextureLock(RHIResource *InTexture, uint32 InMipIndex, uint32 InLayerIndex = 0)
        : Texture(InTexture), MipIndex(InMipIndex), LayerIndex(InLayerIndex) {}
};

inline bool operator==(const TextureLock &A, const TextureLock &B)
{
    return A.Texture == B.Texture && A.MipIndex == B.MipIndex && A.LayerIndex == B.LayerIndex;
}

namespace std
{
    template <>
    struct hash<TextureLock>
    {
        size_t operator()(const TextureLock &value) const
        {
            return GetTypeHash(value.Texture) ^ (value.MipIndex << 16) ^ (value.LayerIndex << 8);
        }
    };
}

static std::unordered_map<TextureLock, VulkanRHI::StagingBuffer *> GPendingLockedBuffers;

// Seperate method for creating VkImageCreateInfo
void VulkanTexture::GenerateImageCreateInfo(
    ImageCreateInfo &OutImageCreateInfo,
    Device &InDevice,
    const TextureDesc &InDesc,
    VkFormat *OutStorageFormat,
    VkFormat *OutViewFormat,
    bool bForceLinearTexture)
{
    const VkPhysicalDeviceProperties &DeviceProperties = InDevice.GetDeviceProperties();
    VkFormat TextureFormat = (VkFormat)GPixelFormats[InDesc.Format].PlatformFormat;

    const TextureCreateFlags UEFlags = InDesc.Flags;
    if (EnumHasAnyFlags(UEFlags, TexCreate_CPUReadback))
    {
        bForceLinearTexture = true;
    }

    VkImageCreateInfo &ImageCreateInfo = OutImageCreateInfo.ImageCreateInfo;
    ZeroVulkanStruct(ImageCreateInfo, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);

    const VkImageViewType ResourceType = UETextureDimensionToVkImageViewType(InDesc.Dimension);
    switch (ResourceType)
    {
    case VK_IMAGE_VIEW_TYPE_1D:
        ImageCreateInfo.imageType = VK_IMAGE_TYPE_1D;
        check((uint32)InDesc.Extent.x <= DeviceProperties.limits.maxImageDimension1D);
        break;
    case VK_IMAGE_VIEW_TYPE_CUBE:
    case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
        check(InDesc.Extent.x == InDesc.Extent.y);
        check((uint32)InDesc.Extent.x <= DeviceProperties.limits.maxImageDimensionCube);
        check((uint32)InDesc.Extent.y <= DeviceProperties.limits.maxImageDimensionCube);
        ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        break;
    case VK_IMAGE_VIEW_TYPE_2D:
    case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
        check((uint32)InDesc.Extent.x <= DeviceProperties.limits.maxImageDimension2D);
        check((uint32)InDesc.Extent.y <= DeviceProperties.limits.maxImageDimension2D);
        ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        break;
    case VK_IMAGE_VIEW_TYPE_3D:
        check((uint32)InDesc.Extent.y <= DeviceProperties.limits.maxImageDimension3D);
        ImageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
        break;
    default:
        printf("Unhandled image type %d", (int32)ResourceType);
        break;
    }

    VkFormat srgbFormat = UEToVkTextureFormat(InDesc.Format, EnumHasAllFlags(UEFlags, TexCreate_SRGB));
    VkFormat nonSrgbFormat = UEToVkTextureFormat(InDesc.Format, false);

    ImageCreateInfo.format = EnumHasAnyFlags(UEFlags, TexCreate_UAV) ? nonSrgbFormat : srgbFormat;

    if (OutViewFormat)
    {
        *OutViewFormat = srgbFormat;
    }
    if (OutStorageFormat)
    {
        *OutStorageFormat = nonSrgbFormat;
    }

    ImageCreateInfo.extent.width = InDesc.Extent.x;
    ImageCreateInfo.extent.height = InDesc.Extent.y;
    ImageCreateInfo.extent.depth = ResourceType == VK_IMAGE_VIEW_TYPE_3D ? InDesc.Depth : 1;
    ImageCreateInfo.mipLevels = InDesc.NumMips;
    const uint32 LayerCount = (ResourceType == VK_IMAGE_VIEW_TYPE_CUBE || ResourceType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) ? 6 : 1;
    ImageCreateInfo.arrayLayers = InDesc.ArraySize * LayerCount;
    check(ImageCreateInfo.arrayLayers <= DeviceProperties.limits.maxImageArrayLayers);

    ImageCreateInfo.flags = (ResourceType == VK_IMAGE_VIEW_TYPE_CUBE || ResourceType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

    const bool bNeedsMutableFormat = (EnumHasAllFlags(UEFlags, TexCreate_SRGB) || (InDesc.Format == PF_R64_UINT));
    if (bNeedsMutableFormat)
    {
        if (/*InDevice.GetOptionalExtensions().HasKHRImageFormatList*/ false)
        {
            printf("Don't support KHRImageFormatList %s %d\n", __FILE__, __LINE__);
            exit(-1);
            // VkImageFormatListCreateInfoKHR &ImageFormatListCreateInfo = OutImageCreateInfo.ImageFormatListCreateInfo;
            // ZeroVulkanStruct(ImageFormatListCreateInfo, VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR);
            // ImageFormatListCreateInfo.pNext = ImageCreateInfo.pNext;
            // ImageCreateInfo.pNext = &ImageFormatListCreateInfo;

            // // Allow non-SRGB views to be created for SRGB textures
            // if (EnumHasAllFlags(UEFlags, TexCreate_SRGB))
            // {
            //     OutImageCreateInfo.FormatsUsed.push_back(nonSrgbFormat);
            //     OutImageCreateInfo.FormatsUsed.push_back(srgbFormat);
            // }

            // // Make it possible to create R32G32 views of R64 images for utilities like clears
            // if (InDesc.Format == PF_R64_UINT)
            // {
            //     OutImageCreateInfo.FormatsUsed.push_back(nonSrgbFormat);
            //     OutImageCreateInfo.FormatsUsed.push_back(UEToVkTextureFormat(PF_R32G32_UINT, false));
            // }

            // ImageFormatListCreateInfo.pViewFormats = OutImageCreateInfo.FormatsUsed.data();
            // ImageFormatListCreateInfo.viewFormatCount = OutImageCreateInfo.FormatsUsed.size();
        }

        ImageCreateInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    }

    if (ImageCreateInfo.imageType == VK_IMAGE_TYPE_3D)
    {
        ImageCreateInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    }

    ImageCreateInfo.tiling = bForceLinearTexture ? VK_IMAGE_TILING_LINEAR : GVulkanViewTypeTilingMode[ResourceType];

    ImageCreateInfo.usage = 0;
    ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    //@TODO: should everything be created with the source bit?
    ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ImageCreateInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

    if (EnumHasAnyFlags(UEFlags, TexCreate_Presentable))
    {
        ImageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    else if (EnumHasAnyFlags(UEFlags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable))
    {
        if (EnumHasAllFlags(UEFlags, TexCreate_InputAttachmentRead))
        {
            ImageCreateInfo.usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        }
        ImageCreateInfo.usage |= (EnumHasAnyFlags(UEFlags, TexCreate_RenderTargetable) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        if (EnumHasAllFlags(UEFlags, TexCreate_Memoryless) && false /*InDevice.GetDeviceMemoryManager().SupportsMemoryless()*/)
        {
            printf("Don't support memoryless %s %d\n", __FILE__, __LINE__);
            exit(-1);
            ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
            // Remove the transfer and sampled bits, as they are incompatible with the transient bit.
            ImageCreateInfo.usage &= ~(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        }
    }
    else if (EnumHasAnyFlags(UEFlags, TexCreate_DepthStencilResolveTarget))
    {
        ImageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    }
    else if (EnumHasAnyFlags(UEFlags, TexCreate_ResolveTargetable))
    {
        ImageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    }

    if (EnumHasAnyFlags(UEFlags, TexCreate_Foveation) /*&& ValidateShadingRateDataType()*/)
    {
        printf("Don't support Variable Shading Rate %s %d\n", __FILE__, __LINE__);
        exit(-1);
        // if (GRHIVariableRateShadingImageDataType == VRSImage_Palette)
        // {
        //     ImageCreateInfo.usage |= VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
        // }

        // if (GRHIVariableRateShadingImageDataType == VRSImage_Fractional)
        // {
        //     ImageCreateInfo.usage |= VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT;
        // }
    }

    if (EnumHasAnyFlags(UEFlags, TexCreate_UAV))
    {
        // cannot have the storage bit on a memoryless texture
        ensure(!EnumHasAnyFlags(UEFlags, TexCreate_Memoryless));
        ImageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    if (EnumHasAnyFlags(UEFlags, TexCreate_External))
    {
        VkExternalMemoryImageCreateInfoKHR &ExternalMemImageCreateInfo = OutImageCreateInfo.ExternalMemImageCreateInfo;
        ZeroVulkanStruct(ExternalMemImageCreateInfo, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR);
#if PLATFORM_WINDOWS
        ExternalMemImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
#else
        ExternalMemImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
#endif
        ExternalMemImageCreateInfo.pNext = ImageCreateInfo.pNext;
        ImageCreateInfo.pNext = &ExternalMemImageCreateInfo;
    }

    // #todo-rco: If using CONCURRENT, make sure to NOT do so on render targets as that kills DCC compression
    ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageCreateInfo.queueFamilyIndexCount = 0;
    ImageCreateInfo.pQueueFamilyIndices = nullptr;

    uint8 NumSamples = InDesc.NumSamples;
    if (ImageCreateInfo.tiling == VK_IMAGE_TILING_LINEAR && NumSamples > 1)
    {
        printf("Warining: Not allowed to create Linear textures with %d samples, reverting to 1 sample\n", NumSamples);
        NumSamples = 1;
    }

    switch (NumSamples)
    {
    case 1:
        ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        break;
    case 2:
        ImageCreateInfo.samples = VK_SAMPLE_COUNT_2_BIT;
        break;
    case 4:
        ImageCreateInfo.samples = VK_SAMPLE_COUNT_4_BIT;
        break;
    case 8:
        ImageCreateInfo.samples = VK_SAMPLE_COUNT_8_BIT;
        break;
    case 16:
        ImageCreateInfo.samples = VK_SAMPLE_COUNT_16_BIT;
        break;
    case 32:
        ImageCreateInfo.samples = VK_SAMPLE_COUNT_32_BIT;
        break;
    case 64:
        ImageCreateInfo.samples = VK_SAMPLE_COUNT_64_BIT;
        break;
    default:
        printf("Unsupported number of samples %d\n", NumSamples);
        break;
    }

    const VkFormatProperties &FormatProperties = InDevice.GetFormatProperties(ImageCreateInfo.format);
    const VkFormatFeatureFlags FormatFlags = ImageCreateInfo.tiling == VK_IMAGE_TILING_LINEAR ? FormatProperties.linearTilingFeatures : FormatProperties.optimalTilingFeatures;

    if (!VKHasAnyFlags(FormatFlags, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
    {
        // Some formats don't support sampling and that's ok, we'll use a STORAGE_IMAGE
        check(EnumHasAnyFlags(UEFlags, TexCreate_UAV | TexCreate_CPUReadback));
        ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    if (!VKHasAnyFlags(FormatFlags, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
    {
        ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_STORAGE_BIT) == 0);
        ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
    }

    if (!VKHasAnyFlags(FormatFlags, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
    {
        ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0);
        ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    if (!VKHasAnyFlags(FormatFlags, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
    {
        ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0);
        ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if (!VKHasAnyFlags(FormatFlags, VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
    {
        // this flag is used unconditionally, strip it without warnings
        ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if (!VKHasAnyFlags(FormatFlags, VK_FORMAT_FEATURE_TRANSFER_DST_BIT))
    {
        // this flag is used unconditionally, strip it without warnings
        ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (EnumHasAnyFlags(UEFlags, TexCreate_DepthStencilTargetable) && GVulkanDepthStencilForceStorageBit)
    {
        ImageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
}

void VulkanTexture::DestroySurface()
{
    const bool bIsLocalOwner = (ImageOwnerType == EImageOwnerType::LocalOwner);
    const bool bHasExternalOwner = (ImageOwnerType == EImageOwnerType::ExternalOwner);

    if (cpuReadbackBuffer)
    {
        printf("Don't support CPU Readback Buffer. %s %d\n", __FILE__, __LINE__);
        exit(-1);
        // device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::DeferredDeletionQueue2::EType::Buffer, CpuReadbackBuffer->Buffer);
        // device->GetMemoryManager().FreeVulkanAllocation(Allocation);
        // delete CpuReadbackBuffer;
    }
    else if (bIsLocalOwner || bHasExternalOwner)
    {
        const bool bRenderTarget = EnumHasAnyFlags(GetDesc().Flags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_ResolveTargetable);
#ifdef PRINT_SEPERATOR_RHI_UNIMPLEMENT
        printf("PRINT_SEPERATOR_RHI_UNIMPLEMENT %s %d\n", __FILE__, __LINE__);
#endif
        device->NotifyDeletedImage(Image, bRenderTarget);

        if (bIsLocalOwner)
        {
            if (Image != VK_NULL_HANDLE)
            {
                device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::DeferredDeletionQueue2::EType::Image, Image);
                vmaFreeMemory(device->GetAllocator(), allocation);
                Image = VK_NULL_HANDLE;
            }
        }

        ImageOwnerType = EImageOwnerType::None;
    }
}

static VkImageLayout GetInitialLayoutFromRHIAccess(Access RHIAccess, bool bIsDepthStencilTarget, bool bSupportReadOnlyOptimal)
{
    if (EnumHasAnyFlags(RHIAccess, Access::RTV) || RHIAccess == Access::Present)
    {
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    if (EnumHasAnyFlags(RHIAccess, Access::DSVWrite))
    {
        return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    }

    if (EnumHasAnyFlags(RHIAccess, Access::DSVRead))
    {
        return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    }

    if (EnumHasAnyFlags(RHIAccess, Access::SRVMask))
    {
        if (bIsDepthStencilTarget)
        {
            return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        }

        return bSupportReadOnlyOptimal ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
    }

    if (EnumHasAnyFlags(RHIAccess, Access::UAVMask))
    {
        return VK_IMAGE_LAYOUT_GENERAL;
    }

    switch (RHIAccess)
    {
    case Access::Unknown:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case Access::Discard:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case Access::CopySrc:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case Access::CopyDest:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case Access::ShadingRateSource:
        printf("Don't support Variable Shading Rate %s %d\n", __FILE__, __LINE__);
    }

    printf("Invalid initial access %d %s %d\n", RHIAccess, __FILE__, __LINE__);
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

VulkanTexture::VulkanTexture(RHICommandListBase *RHICmdList, Device &InDevice, const TextureCreateDesc &InCreateDesc, bool bIsTransientResource)
    : Texture(InCreateDesc), device(&InDevice), Image(VK_NULL_HANDLE),
      StorageFormat(VK_FORMAT_UNDEFINED), ViewFormat(VK_FORMAT_UNDEFINED), MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
{
    if (EnumHasAnyFlags(InCreateDesc.Flags, TexCreate_CPUReadback))
    {
        check(InCreateDesc.NumSamples == 1); // not implemented
        check(InCreateDesc.ArraySize == 1);  // not implemented
        printf("Don't support CPU Readback Texture %s %d\n", __FILE__, __LINE__);
        exit(-1);
    }

    ImageOwnerType = EImageOwnerType::LocalOwner;
    ImageCreateInfo ImageCreateInfo;

    VulkanTexture::GenerateImageCreateInfo(ImageCreateInfo, InDevice, InCreateDesc, &StorageFormat, &ViewFormat);

    FullAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(InCreateDesc.Format, true, true);
    PartialAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(InCreateDesc.Format, false, true);

    VmaAllocationCreateInfo allocationCreateInfo{};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaCreateImage(InDevice.GetAllocator(), &ImageCreateInfo.ImageCreateInfo, &allocationCreateInfo, &Image, &allocation, &allocationInfo);

    Tiling = ImageCreateInfo.ImageCreateInfo.tiling;
    check(Tiling == VK_IMAGE_TILING_LINEAR || Tiling == VK_IMAGE_TILING_OPTIMAL);
    ImageUsageFlags = ImageCreateInfo.ImageCreateInfo.usage;

    const bool bRenderTarget = EnumHasAnyFlags(InCreateDesc.Flags,
                                               TexCreate_RenderTargetable |
                                                   TexCreate_DepthStencilTargetable |
                                                   TexCreate_ResolveTargetable);
    const VkImageLayout InitialLayout = GetInitialLayoutFromRHIAccess(InCreateDesc.InitialState, bRenderTarget && IsDepthOrStencilAspect(), SupportsSampling());

    const bool bDoInitialClear = VKHasAnyFlags(ImageCreateInfo.ImageCreateInfo.usage,
                                               VK_IMAGE_USAGE_SAMPLED_BIT) &&
                                 EnumHasAnyFlags(InCreateDesc.Flags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable);

    if (InitialLayout != VK_IMAGE_LAYOUT_UNDEFINED || bDoInitialClear)
    {
        SetInitialImageState(device->GetImmediateContext(), InitialLayout, bDoInitialClear, InCreateDesc.ClearValue, bIsTransientResource);
    }

    DefaultLayout = InitialLayout;

    const VkImageViewType ViewType = GetViewType();
    const bool bIsSRGB = EnumHasAllFlags(InCreateDesc.Flags, TexCreate_SRGB);
    if (ViewFormat == VK_FORMAT_UNDEFINED)
    {
        StorageFormat = UEToVkTextureFormat(InCreateDesc.Format, false);
        ViewFormat = UEToVkTextureFormat(InCreateDesc.Format, bIsSRGB);
        check(StorageFormat != VK_FORMAT_UNDEFINED);
    }

    const VkDescriptorType DescriptorType = SupportsSampling() ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    const VkImageUsageFlags SRVUsage = bIsSRGB ? (ImageCreateInfo.ImageCreateInfo.usage & ~VK_IMAGE_USAGE_STORAGE_BIT) : ImageCreateInfo.ImageCreateInfo.usage;
    if (ViewType != VK_IMAGE_VIEW_TYPE_MAX_ENUM)
    {
        DefaultView = (new View(InDevice, DescriptorType))->InitAsTextureView(Image, ViewType, GetFullAspectMask(), InCreateDesc.Format, ViewFormat, 0, std::max(InCreateDesc.NumMips, (uint8)1u), 0, GetNumberOfArrayLevels(), !SupportsSampling(), SRVUsage);
    }

    if (FullAspectMask == PartialAspectMask)
    {
        PartialView = DefaultView;
    }
    else
    {
        PartialView = (new View(InDevice, DescriptorType))->InitAsTextureView(Image, ViewType, PartialAspectMask, InCreateDesc.Format, ViewFormat, 0, std::max(InCreateDesc.NumMips, (uint8)1u), 0, GetNumberOfArrayLevels(), false);
    }

    if (!InCreateDesc.BulkData)
    {
        return;
    }

    // InternalLockWrite leaves the image in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, so make sure the requested resource state is SRV.
    check(EnumHasAnyFlags(InCreateDesc.InitialState, Access::SRVMask));
    DefaultLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Transfer bulk data
    VulkanRHI::StagingBuffer *StagingBuffer = InDevice.GetStagingManager().AcquireBuffer(InCreateDesc.BulkDataSize);
    void *Data = StagingBuffer->GetMappedPointer();

    // Do copy
    memcpy(Data, InCreateDesc.BulkData, InCreateDesc.BulkDataSize);
    delete[] InCreateDesc.BulkData;

    VkBufferImageCopy Region{};
    // #todo-rco: Use real Buffer offset when switching to suballocations!
    Region.bufferOffset = 0;
    Region.bufferRowLength = InCreateDesc.Extent.x;
    Region.bufferImageHeight = InCreateDesc.Extent.y;

    Region.imageSubresource.mipLevel = 0;
    Region.imageSubresource.baseArrayLayer = 0;
    Region.imageSubresource.layerCount = GetNumberOfArrayLevels();
    Region.imageSubresource.aspectMask = GetFullAspectMask();

    Region.imageExtent.width = Region.bufferRowLength;
    Region.imageExtent.height = Region.bufferImageHeight;
    Region.imageExtent.depth = InCreateDesc.Depth;

    VulkanTexture::InternalLockWrite(InDevice.GetImmediateContext(), this, Region, StagingBuffer);
}

void VulkanTexture::InternalLockWrite(CommandListContext &Context, VulkanTexture *Surface,
                                      const VkBufferImageCopy &Region, VulkanRHI::StagingBuffer *StagingBuffer)
{
    CmdBuffer *CmdBuffer = Context.GetCommandBufferManager()->GetUploadCmdBuffer();
    ensure(CmdBuffer->IsOutsideRenderPass());
    VkCommandBuffer StagingCommandBuffer = CmdBuffer->GetHandle();

    const VkImageSubresourceLayers &ImageSubresource = Region.imageSubresource;
    const VkImageSubresourceRange SubresourceRange = PipelineBarrier::MakeSubresourceRange(ImageSubresource.aspectMask,
                                                                                           ImageSubresource.mipLevel, 1,
                                                                                           ImageSubresource.baseArrayLayer,
                                                                                           ImageSubresource.layerCount);

    {
        PipelineBarrier Barrier;
        Barrier.AddImageLayoutTransition(Surface->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);
        Barrier.Execute(CmdBuffer);
    }

    vkCmdCopyBufferToImage(StagingCommandBuffer, StagingBuffer->GetHandle(), Surface->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

    {
        PipelineBarrier Barrier;
        Barrier.AddImageLayoutTransition(Surface->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Surface->GetDefaultLayout(), SubresourceRange);
        Barrier.Execute(CmdBuffer);
    }

    Surface->device->GetStagingManager().ReleaseBuffer(CmdBuffer, StagingBuffer);

    Context.GetCommandBufferManager()->SubmitUploadCmdBuffer();
}

void VulkanTexture::GetMipStride(uint32 MipIndex, uint32 &Stride)
{
    // Calculate the width of the MipMap.
    const TextureDesc &Desc = GetDesc();
    const PixelFormat PixelFormat = Desc.Format;
    const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
    const uint32 MipSizeX = std::max<uint32>(Desc.Extent.x >> MipIndex, BlockSizeX);
    uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
    if (PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4)
    {
        // PVRTC has minimum 2 blocks width
        NumBlocksX = std::max<uint32>(NumBlocksX, 2);
    }
    const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
    Stride = NumBlocksX * BlockBytes;
}

void VulkanTexture::GetMipOffset(uint32 MipIndex, uint32 &Offset)
{
    uint32 offset = Offset = 0;
    for (uint32 i = 0; i < MipIndex; i++)
    {
        GetMipSize(i, offset);
        Offset += offset;
    }
}

void VulkanTexture::GetMipSize(uint32 MipIndex, uint32 &MipBytes)
{
    // Calculate the dimensions of mip-map level.
    const TextureDesc &Desc = GetDesc();
    const PixelFormat PixelFormat = Desc.Format;
    const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
    const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
    const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
    const uint32 MipSizeX = std::max<uint32>(Desc.Extent.x >> MipIndex, BlockSizeX);
    const uint32 MipSizeY = std::max<uint32>(Desc.Extent.y >> MipIndex, BlockSizeY);
    uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
    uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;

    if (PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4)
    {
        // PVRTC has minimum 2 blocks width and height
        NumBlocksX = std::max<uint32>(NumBlocksX, 2);
        NumBlocksY = std::max<uint32>(NumBlocksY, 2);
    }

    // Size in bytes
    MipBytes = NumBlocksX * NumBlocksY * BlockBytes * Desc.Depth;
}

void VulkanTexture::SetInitialImageState(CommandListContext &Context, VkImageLayout InitialLayout,
                                         bool bClear, const ClearValueBinding &ClearValueBinding, bool bIsTransientResource)
{
    // Can't use TransferQueue as Vulkan requires that queue to also have Gfx or Compute capabilities...
    // #todo-rco: This function is only used during loading currently, if used for regular RHIClear then use the ActiveCmdBuffer
    // NOTE: Transient resources' memory might have belonged to another resource earlier in the ActiveCmdBuffer, so we can't use UploadCmdBuffer
    CmdBuffer *CmdBuffer = bIsTransientResource ? Context.GetCommandBufferManager()->GetActiveCmdBuffer() : Context.GetCommandBufferManager()->GetUploadCmdBuffer();
    ensure(CmdBuffer->IsOutsideRenderPass());

    VkImageSubresourceRange SubresourceRange = PipelineBarrier::MakeSubresourceRange(FullAspectMask);

    VkImageLayout CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (bClear && !bIsTransientResource)
    {
        {
            PipelineBarrier Barrier;
            Barrier.AddImageLayoutTransition(Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);
            Barrier.Execute(CmdBuffer);
        }

        if (FullAspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
        {
            VkClearColorValue Color{};
            Color.float32[0] = ClearValueBinding.Value.Color[0];
            Color.float32[1] = ClearValueBinding.Value.Color[1];
            Color.float32[2] = ClearValueBinding.Value.Color[2];
            Color.float32[3] = ClearValueBinding.Value.Color[3];

            vkCmdClearColorImage(CmdBuffer->GetHandle(), Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &Color, 1, &SubresourceRange);
        }
        else
        {
            check(IsDepthOrStencilAspect());
            VkClearDepthStencilValue Value{};
            Value.depth = ClearValueBinding.Value.DSValue.Depth;
            Value.stencil = ClearValueBinding.Value.DSValue.Stencil;

            vkCmdClearDepthStencilImage(CmdBuffer->GetHandle(), Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &Value, 1, &SubresourceRange);
        }

        CurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }

    if ((InitialLayout != CurrentLayout) && (InitialLayout != VK_IMAGE_LAYOUT_UNDEFINED))
    {
        PipelineBarrier Barrier;
        Barrier.AddFullImageLayoutTransition(*this, CurrentLayout, InitialLayout);
        Barrier.Execute(CmdBuffer);
    }

    CmdBuffer->GetLayoutManager().SetFullLayout(*this, InitialLayout);
}

/*-----------------------------------------------------------------------------
    2D texture support.
-----------------------------------------------------------------------------*/

// 1015
std::shared_ptr<Texture> RHI::CreateTexture(RHICommandListBase &RHICmdList, const TextureCreateDesc &CreateDesc)
{
    Texture *tex = new VulkanTexture(&RHICmdList, *device, CreateDesc);
    return std::shared_ptr<Texture>(tex);
}

// 1183
void *RHI::LockTexture2D(Texture *TextureRHI, uint32 MipIndex, ResourceLockMode LockMode, uint32 &DestStride, bool bLockWithinMiptail, uint64 *OutLockedByteCount)
{
    VulkanTexture *texture = static_cast<VulkanTexture *>(TextureRHI);
    check(texture);

    VulkanRHI::StagingBuffer **StagingBuffer = nullptr;
    {
        /* FScopeLock Lock(&GTextureMapLock); */
        TextureLock lock(TextureRHI, MipIndex);
        auto it = GPendingLockedBuffers.find(lock);
        if (it == GPendingLockedBuffers.end())
        {
            GPendingLockedBuffers.insert(std::pair(lock, nullptr));
        }
        StagingBuffer = &GPendingLockedBuffers[lock];

        checkf(!StagingBuffer, "Can't lock the same texture twice!");
    }

    // No locks for read allowed yet
    check(LockMode == RLM_WriteOnly);

    uint32 BufferSize = 0;
    DestStride = 0;
    texture->GetMipSize(MipIndex, BufferSize);
    texture->GetMipStride(MipIndex, DestStride);
    *StagingBuffer = device->GetStagingManager().AcquireBuffer(BufferSize);

    if (OutLockedByteCount)
    {
        *OutLockedByteCount = BufferSize;
    }

    void *Data = (*StagingBuffer)->GetMappedPointer();
    return Data;
}

void RHI::InternalUnlockTexture2D(bool bFromRenderingThread, Texture *TextureRHI, uint32 MipIndex, bool bLockWithinMiptail)
{
    VulkanTexture *texture = static_cast<VulkanTexture *>(TextureRHI);
    check(texture);

    VkDevice LogicalDevice = device->GetInstanceHandle();

    VulkanRHI::StagingBuffer *StagingBuffer = nullptr;
    {
        /* FScopeLock Lock(&GTextureMapLock); */
        auto it = GPendingLockedBuffers.find(TextureLock(TextureRHI, MipIndex));
        bool bFound = it != GPendingLockedBuffers.end();
        if (bFound)
        {
            StagingBuffer = it->second;
            GPendingLockedBuffers.erase(it);
        }
        checkf(bFound, "Texture was not locked!");
    }

    const TextureDesc &Desc = texture->GetDesc();
    const PixelFormat Format = Desc.Format;
    uint32 MipWidth = std::max<uint32>(Desc.Extent.x >> MipIndex, 0);
    uint32 MipHeight = std::max<uint32>(Desc.Extent.x >> MipIndex, 0);
    ensure(!(MipHeight == 0 && MipWidth == 0));
    MipWidth = std::max<uint32>(MipWidth, 1);
    MipHeight = std::max<uint32>(MipHeight, 1);
    uint32 LayerCount = texture->GetNumberOfArrayLevels();

    VkBufferImageCopy Region;
    memset(&Region, 0, sizeof(Region));
    // #todo-rco: Might need an offset here?
    // Region.bufferOffset = 0;
    Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Region.imageSubresource.mipLevel = MipIndex;
    Region.imageSubresource.layerCount = LayerCount;
    Region.imageExtent.width = MipWidth;
    Region.imageExtent.height = MipHeight;
    Region.imageExtent.depth = 1;

    RHICommandList &RHICmdList = RHICommandListExecutor::GetImmediateCommandList();
    if (!bFromRenderingThread || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
    {
        VulkanTexture::InternalLockWrite(device->GetImmediateContext(), texture, Region, StagingBuffer);
    }
    else
    {
        check(0);
        /* check(IsInRenderingThread());
        ALLOC_COMMAND_CL(RHICmdList, FRHICommandLockWriteTexture)
        (texture, Region, StagingBuffer); */
    }
}
