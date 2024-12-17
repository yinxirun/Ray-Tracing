#include "resources.h"
#include "device.h"
#include "rhi.h"
#include "descriptor_sets.h"
#include "../definitions.h"
#include "configuration.h"
#include "util.h"
#include "private.h"

VulkanView::VulkanView(Device &InDevice, VkDescriptorType InDescriptorType) : device(InDevice)
{
    BindlessHandle = device.GetBindlessDescriptorManager()->ReserveDescriptor(InDescriptorType);
}

VulkanView::~VulkanView()
{
    Invalidate();

    if (BindlessHandle.IsValid())
    {
        printf("Fuck!! %s %s\n", __FILE__, __LINE__);
        // device.GetDeferredDeletionQueue().EnqueueBindlessHandle(BindlessHandle);
        // BindlessHandle = RHIDescriptorHandle();
    }
}

void VulkanView::Invalidate()
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

VulkanView *VulkanView::InitAsTypedBufferView(VulkanMultiBuffer *Buffer, PixelFormat UEFormat, uint32 InOffset, uint32 InSize)
{
    // todo
    check(0);
    return nullptr;
}

VulkanView *VulkanView::InitAsTextureView(VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, PixelFormat UEFormat,
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

VulkanView *VulkanView::InitAsStructuredBufferView(VulkanMultiBuffer *Buffer, uint32 InOffset, uint32 InSize)
{
    // We will need a deferred update if the descriptor was already in use
    const bool bImmediateUpdate = !IsInitialized();

    check(GetViewType() == EType::Null);
    viewType = EType::StructuredBuffer;
    StructuredBufferView &SBV = structuredBufferView;

    const uint32 TotalOffset = Buffer->GetOffset() + InOffset;

    SBV.Buffer = Buffer->GetHandle();
    SBV.HandleId = (uint64)Buffer->GetCurrentAllocation();
    SBV.Offset = TotalOffset;

    // :todo-jn: Volatile buffers use temporary allocations that can be smaller than the buffer creation size.  Check if the savings are still worth it.
    if (Buffer->IsVolatile())
    {
        InSize = std::min<uint64>(InSize, Buffer->GetCurrentSize());
    }

    SBV.Size = InSize;

    device.GetBindlessDescriptorManager()->UpdateBuffer(BindlessHandle, Buffer->GetHandle(), TotalOffset, InSize, bImmediateUpdate);

    return this;
}

// 276
void VulkanViewableResource::UpdateLinkedViews()
{
    for (VulkanLinkedView *view = LinkedViews; view; /*view = view->Next()*/)
    {
        printf("ERROR: Don't support LinkedView %s %d\n", __FILE__, __LINE__);
        exit(-1);
        // view->UpdateView();
    }
}

static VkDescriptorType GetDescriptorTypeForViewDesc(ViewDesc const &ViewDesc)
{
    if (ViewDesc.IsBuffer())
    {
        if (ViewDesc.IsSRV())
        {
            switch (ViewDesc.buffer.SRV.bufferType)
            {
            case ViewDesc::BufferType::Raw:
            case ViewDesc::BufferType::Structured:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

            case ViewDesc::BufferType::Typed:
                return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;

            case ViewDesc::BufferType::AccelerationStructure:
                return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

            default:
                checkNoEntry();
                return VK_DESCRIPTOR_TYPE_MAX_ENUM;
            }
        }
        else
        {
            switch (ViewDesc.buffer.UAV.bufferType)
            {
            case ViewDesc::BufferType::Raw:
            case ViewDesc::BufferType::Structured:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

            case ViewDesc::BufferType::Typed:
                return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;

            case ViewDesc::BufferType::AccelerationStructure:
                return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

            default:
                checkNoEntry();
                return VK_DESCRIPTOR_TYPE_MAX_ENUM;
            }
        }
    }
    else
    {
        check(0);
        /*         if (ViewDesc.IsSRV())
                {
                    // Sampled images aren't supported in R64, shadercompiler patches them to storage image
                    if (ViewDesc.common.Format == PF_R64_UINT)
                    {
                        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    }

                    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                }
                else
                {
                    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                } */
    }
}

void VulkanUnorderedAccessView::UpdateView()
{
    Invalidate();

    if (IsBuffer())
    {
        VulkanMultiBuffer *Buffer = static_cast<VulkanMultiBuffer *>(GetBuffer());
        auto const Info = viewDesc.buffer.UAV.GetViewInfo(Buffer);

        checkf(!Info.bAppendBuffer && !Info.bAtomicCounter, "UAV counters not implemented in Vulkan RHI.");

        if (!Info.bNullView)
        {
            switch (Info.BufferType)
            {
            case ViewDesc::BufferType::Raw:
            case ViewDesc::BufferType::Structured:
                InitAsStructuredBufferView(Buffer, Info.OffsetInBytes, Info.SizeInBytes);
                break;

            case ViewDesc::BufferType::Typed:
                check(VKHasAllFlags(Buffer->GetBufferUsageFlags(), VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT));
                InitAsTypedBufferView(Buffer, Info.Format, Info.OffsetInBytes, Info.SizeInBytes);
                break;

#if VULKAN_RHI_RAYTRACING
            case FRHIViewDesc::EBufferType::AccelerationStructure:
                checkNoEntry(); // @todo implement
                break;
#endif

            default:
                checkNoEntry();
                break;
            }
        }
    }
    else
    {
        check(0);
        /* 		VulkanTexture* Texture = static_cast<VulkanTexture*>(GetTexture());
                auto const Info = viewDesc.Texture.UAV.GetViewInfo(Texture);

                uint32 ArrayFirst = Info.ArrayRange.First;
                uint32 ArrayNum = Info.ArrayRange.Num;
                if (Info.Dimension == ViewDesc::Dimension::TextureCube || Info.Dimension == ViewDesc::Dimension::TextureCubeArray)
                {
                    ArrayFirst *= 6;
                    ArrayNum *= 6;
                    checkf((ArrayFirst + ArrayNum) <= Texture->GetNumberOfArrayLevels(), "View extends beyond original cube texture level count!");
                }

                InitAsTextureView(
                      Texture->Image
                    , GetVkImageViewTypeForDimensionUAV(Info.Dimension, Texture->GetViewType())
                    , Texture->GetPartialAspectMask()
                    , Info.Format
                    , UEToVkTextureFormat(Info.Format, false)
                    , Info.MipLevel
                    , 1
                    , ArrayFirst
                    , ArrayNum
                    , true
                ); */
    }
}

VulkanUnorderedAccessView::VulkanUnorderedAccessView(RHICommandListBase &RHICmdList, Device &InDevice,
                                                     ViewableResource *InResource, ViewDesc const &InViewDesc)
    : UnorderedAccessView(InResource, InViewDesc), VulkanLinkedView(InDevice, GetDescriptorTypeForViewDesc(InViewDesc))
{
    /* RHICmdList.EnqueueLambda([this](FRHICommandListBase&)
    {
        LinkHead(GetBaseResource()->LinkedViews);
        UpdateView();
    }); */

    UpdateView();
}

std::shared_ptr<UnorderedAccessView> RHI::CreateUnorderedAccessView(class RHICommandListBase &RHICmdList,
                                                                    ViewableResource *Resource, ViewDesc const &ViewDesc)
{
    UnorderedAccessView *view = new VulkanUnorderedAccessView(RHICmdList, *device, Resource, ViewDesc);
    return std::shared_ptr<UnorderedAccessView>(view);
}
