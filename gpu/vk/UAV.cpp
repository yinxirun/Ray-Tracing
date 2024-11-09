#include "resources.h"
#include "device.h"
#include "descriptor_sets.h"
#include "../definitions.h"
#include "configuration.h"
#include "util.h"
#include "private.h"

View::View(Device &InDevice, VkDescriptorType InDescriptorType) : device(InDevice)
{
    BindlessHandle = device.GetBindlessDescriptorManager()->ReserveDescriptor(InDescriptorType);
}

View::~View()
{
    Invalidate();

    if (BindlessHandle.IsValid())
    {
        printf("Fuck!! %s %s\n", __FILE__, __LINE__);
        // device.GetDeferredDeletionQueue().EnqueueBindlessHandle(BindlessHandle);
        // BindlessHandle = RHIDescriptorHandle();
    }
}

void View::Invalidate()
{
    // Carry forward its initialized state
    const bool bIsInitialized = IsInitialized();

    switch (GetViewType())
    {
    // default: checkNoEntry(); [[fallthrough]];
    case EType::Null:
        break;

    case EType::TypedBuffer:
        // DEC_DWORD_STAT(STAT_VulkanNumBufferViews);
        device.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::DeferredDeletionQueue2::EType::BufferView, typedBufferView.View);
        break;

    case EType::Texture:
        // DEC_DWORD_STAT(STAT_VulkanNumImageViews);
        device.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::DeferredDeletionQueue2::EType::ImageView, textureView.View);
        break;

    case EType::StructuredBuffer:
        // Nothing to do
        break;

#if VULKAN_RHI_RAYTRACING
    case EType::AccelerationStructure:
        Device.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::AccelerationStructure, Storage.Get<FAccelerationStructureView>().Handle);
        break;
#endif
    }

    viewType = EType::Null;
    invalidatedState.bInitialized = bIsInitialized;
}

View *View::InitAsTextureView(VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, PixelFormat UEFormat,
                              VkFormat Format, uint32_t FirstMip, uint32_t NumMips, uint32_t ArraySliceIndex, uint32_t NumArraySlices, bool bUseIdentitySwizzle,
                              VkImageUsageFlags ImageUsageFlags)
{
    // We will need a deferred update if the descriptor was already in use
    const bool bImmediateUpdate = !IsInitialized();

    check(GetViewType() == EType::Null);
    viewType = EType::Texture;
    TextureView &TV = textureView;

    // LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);

    VkImageViewCreateInfo ViewInfo;
    ZeroVulkanStruct(ViewInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    ViewInfo.image = InImage;
    ViewInfo.viewType = ViewType;
    ViewInfo.format = Format;

#if VULKAN_SUPPORTS_ASTC_DECODE_MODE
    VkImageViewASTCDecodeModeEXT DecodeMode;
    if (Device.GetOptionalExtensions().HasEXTASTCDecodeMode && IsAstcLdrFormat(Format) && !IsAstcSrgbFormat(Format))
    {
        ZeroVulkanStruct(DecodeMode, VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT);
        DecodeMode.decodeMode = VK_FORMAT_R8G8B8A8_UNORM;
        DecodeMode.pNext = ViewInfo.pNext;
        ViewInfo.pNext = &DecodeMode;
    }
#endif

    if (bUseIdentitySwizzle)
    {
        ViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    }
    else
    {
        ViewInfo.components = device.GetFormatComponentMapping(UEFormat);
    }

    ViewInfo.subresourceRange.aspectMask = AspectFlags;
    ViewInfo.subresourceRange.baseMipLevel = FirstMip;
    ensure(NumMips != 0xFFFFFFFF);
    ViewInfo.subresourceRange.levelCount = NumMips;

    ensure(ArraySliceIndex != 0xFFFFFFFF);
    ensure(NumArraySlices != 0xFFFFFFFF);
    ViewInfo.subresourceRange.baseArrayLayer = ArraySliceIndex;
    ViewInfo.subresourceRange.layerCount = NumArraySlices;

    // HACK.  DX11 on PC currently uses a D24S8 depthbuffer and so needs an X24_G8 SRV to visualize stencil.
    // So take that as our cue to visualize stencil.  In the future, the platform independent code will have a real format
    // instead of PF_DepthStencil, so the cross-platform code could figure out the proper format to pass in for this.
    if (/*UEFormat == PF_X24_G8*/ false)
    {
        // ensure((ViewInfo.format == (VkFormat)GPixelFormats[PF_DepthStencil].PlatformFormat) && (ViewInfo.format != VK_FORMAT_UNDEFINED));
        // ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        printf("Don't support PF_X24_G8 Format %s: %s\n", __FILE__, __LINE__);
    }

    // Inform the driver the view will only be used with a subset of usage flags (to help performance and/or compatibility)
    VkImageViewUsageCreateInfo ImageViewUsageCreateInfo;
    if (ImageUsageFlags != 0)
    {
        ZeroVulkanStruct(ImageViewUsageCreateInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO);
        ImageViewUsageCreateInfo.usage = ImageUsageFlags;

        ImageViewUsageCreateInfo.pNext = (void *)ViewInfo.pNext;
        ViewInfo.pNext = &ImageViewUsageCreateInfo;
    }

    // INC_DWORD_STAT(STAT_VulkanNumImageViews);
    VERIFYVULKANRESULT(vkCreateImageView(device.GetInstanceHandle(), &ViewInfo, VULKAN_CPU_ALLOCATOR, &TV.View));

    TV.Image = InImage;

    if (UseVulkanDescriptorCache())
    {
        printf("Fuck!! %s: %s\n", __FILE__, __LINE__);
        // TV.ViewId = ++GVulkanImageViewHandleIdCounter;
    }

    const bool bDepthOrStencilAspect = (AspectFlags & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
    device.GetBindlessDescriptorManager()->UpdateImage(BindlessHandle, TV.View, bDepthOrStencilAspect, bImmediateUpdate);

    return this;
}

// 276
void VulkanViewableResource::UpdateLinkedViews()
{
    for (LinkedView *view = LinkedViews; view; /*view = view->Next()*/)
    {
        printf("ERROR: Don't support LinkedView %s %d\n", __FILE__, __LINE__);
        exit(-1);
        // view->UpdateView();
    }
}