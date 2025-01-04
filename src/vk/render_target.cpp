#include "renderpass.h"
#include "private.h"
#include "device.h"
#include "context.h"
#include "resources.h"
#include "platform.h"
#include "rhi.h"
#include "pending_state.h"
#include "command_buffer.h"
#include "viewport.h"
#include "RHI/RHIResources.h"
#include "core/containers/enum_as_byte.h"
#include "core/misc/crc.h"
#include <vector>

extern RHI *rhi;
/// 1 to submit the cmd buffer after end occlusion query batch (default)
int GSubmitOcclusionBatchCmdBufferCVar = 1;

RenderPass *CommandListContext::PrepareRenderPassForPSOCreation(const GraphicsPipelineStateInitializer &Initializer)
{
    RenderTargetLayout RTLayout(Initializer);
    return PrepareRenderPassForPSOCreation(RTLayout);
}

RenderPass *CommandListContext::PrepareRenderPassForPSOCreation(const RenderTargetLayout &RTLayout)
{
    RenderPass *RenderPass = nullptr;
    RenderPass = device->GetRenderPassManager().GetOrCreateRenderPass(RTLayout);
    return RenderPass;
}

VkSurfaceTransformFlagBitsKHR CommandListContext::GetSwapchainQCOMRenderPassTransform() const
{
    std::vector<VulkanViewport *> &viewports = rhi->GetViewports();
    if (viewports.size() == 0)
    {
        return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    return viewports[0]->GetSwapchainQCOMRenderPassTransform();
}

SwapChain *CommandListContext::GetSwapChain() const
{
    std::vector<VulkanViewport *> &viewports = rhi->GetViewports();
    uint32 numViewports = viewports.size();
    if (viewports.size() == 0)
        return nullptr;
    return viewports[0]->GetSwapChain();
}

bool CommandListContext::IsSwapchainImage(Texture *InTexture) const
{
    std::vector<VulkanViewport *> &Viewports = rhi->GetViewports();
    uint32 NumViewports = Viewports.size();

    for (uint32 i = 0; i < NumViewports; i++)
    {
        VkImage Image = dynamic_cast<VulkanTexture *>(InTexture)->Image;
        uint32 BackBufferImageCount = Viewports[i]->GetBackBufferImageCount();

        for (uint32 SwapchainImageIdx = 0; SwapchainImageIdx < BackBufferImageCount; SwapchainImageIdx++)
        {
            if (Image == Viewports[i]->GetBackBufferImage(SwapchainImageIdx))
            {
                return true;
            }
        }
    }
    return false;
}

// Need a separate struct so we can memzero/remove dependencies on reference counts
struct RenderPassCompatibleHashableStruct
{
    RenderPassCompatibleHashableStruct()
    {
        memset(this, 0, sizeof(RenderPassCompatibleHashableStruct));
    }

    uint8 NumAttachments;
    uint8 MultiViewCount;
    uint8 NumSamples;
    uint8 SubpassHint;
    VkSurfaceTransformFlagBitsKHR QCOMRenderPassTransform;
    // +1 for Depth, +1 for Stencil, +1 for Fragment Density
    VkFormat Formats[MaxSimultaneousRenderTargets + 3];
    uint16 AttachmentsToResolve;
};

// Need a separate struct so we can memzero/remove dependencies on reference counts
struct RenderPassFullHashableStruct
{
    RenderPassFullHashableStruct()
    {
        memset(this, 0, sizeof(RenderPassFullHashableStruct));
    }

    // +1 for Depth, +1 for Stencil, +1 for Fragment Density
    TEnumAsByte<VkAttachmentLoadOp> LoadOps[MaxSimultaneousRenderTargets + 3];
    TEnumAsByte<VkAttachmentStoreOp> StoreOps[MaxSimultaneousRenderTargets + 3];
    // If the initial != final we need to add FinalLayout and potentially RefLayout
    VkImageLayout InitialLayout[MaxSimultaneousRenderTargets + 3];
    // VkImageLayout						FinalLayout[MaxSimultaneousRenderTargets + 3];
    // VkImageLayout						RefLayout[MaxSimultaneousRenderTargets + 3];
};

RenderTargetLayout::RenderTargetLayout(Device &InDevice, const RenderPassInfo &RPInfo,
                                       VkImageLayout CurrentDepthLayout, VkImageLayout CurrentStencilLayout)
    : NumAttachmentDescriptions(0), NumColorAttachments(0), bHasDepthStencil(false),
      bHasResolveAttachments(false), bHasFragmentDensityAttachment(false), NumSamples(0),
      NumUsedClearValues(0), MultiViewCount(RPInfo.MultiViewCount)
{
    ResetAttachments();

    RenderPassCompatibleHashableStruct CompatibleHashInfo;
    RenderPassFullHashableStruct FullHashInfo;

    bool bSetExtent = false;
    bool bFoundClearOp = false;
    bool bMultiviewRenderTargets = false;

    int32 NumColorRenderTargets = RPInfo.GetNumColorRenderTargets();
    for (int32 Index = 0; Index < NumColorRenderTargets; ++Index)
    {
        const RenderPassInfo::ColorEntry &ColorEntry = RPInfo.ColorRenderTargets[Index];
        VulkanTexture *texture = dynamic_cast<VulkanTexture *>(ColorEntry.RenderTarget);
        check(texture);
        const TextureDesc &TextureDesc = texture->GetDesc();

        if (InDevice.GetImmediateContext().IsSwapchainImage(ColorEntry.RenderTarget))
        {
            QCOMRenderPassTransform = InDevice.GetImmediateContext().GetSwapchainQCOMRenderPassTransform();
        }
        check(QCOMRenderPassTransform == VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR || NumAttachmentDescriptions == 0);

        if (bSetExtent)
        {
            ensure(Extent.Extent3D.width == std::max(1, TextureDesc.Extent.x >> ColorEntry.MipIndex));
            ensure(Extent.Extent3D.height == std::max(1, TextureDesc.Extent.y >> ColorEntry.MipIndex));
            ensure(Extent.Extent3D.depth == TextureDesc.Depth);
        }
        else
        {
            bSetExtent = true;
            Extent.Extent3D.width = std::max(1, TextureDesc.Extent.x >> ColorEntry.MipIndex);
            Extent.Extent3D.height = std::max(1, TextureDesc.Extent.y >> ColorEntry.MipIndex);
            Extent.Extent3D.depth = TextureDesc.Depth;
        }

        // CustomResolveSubpass can have targets with a different NumSamples
        ensure(!NumSamples || NumSamples == ColorEntry.RenderTarget->GetDesc().NumSamples || RPInfo.SubpassHint == SubpassHint::CustomResolveSubpass);
        NumSamples = ColorEntry.RenderTarget->GetDesc().NumSamples;

        ensure(!GetIsMultiView() || !bMultiviewRenderTargets || texture->GetNumberOfArrayLevels() > 1);
        bMultiviewRenderTargets = texture->GetNumberOfArrayLevels() > 1;
        // With a CustomResolveSubpass last color attachment is a resolve target
        bool bCustomResolveAttachment = (Index == (NumColorRenderTargets - 1)) && RPInfo.SubpassHint == SubpassHint::CustomResolveSubpass;

        VkAttachmentDescription &CurrDesc = Desc[NumAttachmentDescriptions];
        CurrDesc.samples = bCustomResolveAttachment ? VK_SAMPLE_COUNT_1_BIT : static_cast<VkSampleCountFlagBits>(NumSamples);
        CurrDesc.format = UEToVkTextureFormat(ColorEntry.RenderTarget->GetDesc().Format, EnumHasAllFlags(texture->GetDesc().Flags, TexCreate_SRGB));
        CurrDesc.loadOp = RenderTargetLoadActionToVulkan(GetLoadAction(ColorEntry.Action));
        bFoundClearOp = bFoundClearOp || (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
        CurrDesc.storeOp = RenderTargetStoreActionToVulkan(GetStoreAction(ColorEntry.Action));
        CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        if (EnumHasAnyFlags(texture->GetDesc().Flags, TexCreate_Memoryless))
        {
            ensure(CurrDesc.storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
        }

        // If the initial != final we need to change the FullHashInfo and use FinalLayout
        CurrDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        CurrDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        ColorReferences[NumColorAttachments].attachment = NumAttachmentDescriptions;
        ColorReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT && ColorEntry.ResolveTarget)
        {
            Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
            Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
            Desc[NumAttachmentDescriptions + 1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            Desc[NumAttachmentDescriptions + 1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            ResolveReferences[NumColorAttachments].attachment = NumAttachmentDescriptions + 1;
            ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            CompatibleHashInfo.AttachmentsToResolve |= (uint16)(1 << NumColorAttachments);
            ++NumAttachmentDescriptions;
            bHasResolveAttachments = true;
        }

        CompatibleHashInfo.Formats[NumColorAttachments] = CurrDesc.format;
        FullHashInfo.LoadOps[NumColorAttachments] = CurrDesc.loadOp;
        FullHashInfo.InitialLayout[NumColorAttachments] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        FullHashInfo.StoreOps[NumColorAttachments] = CurrDesc.storeOp;
        ++CompatibleHashInfo.NumAttachments;

        ++NumAttachmentDescriptions;
        ++NumColorAttachments;
    }

    if (RPInfo.DepthStencilRenderTarget.DepthStencilTarget)
    {
        VkAttachmentDescription &CurrDesc = Desc[NumAttachmentDescriptions];
        Memzero(CurrDesc);
        VulkanTexture *texture = dynamic_cast<VulkanTexture *>(RPInfo.DepthStencilRenderTarget.DepthStencilTarget);
        check(texture);
        const TextureDesc &TextureDesc = texture->GetDesc();

        CurrDesc.samples = static_cast<VkSampleCountFlagBits>(RPInfo.DepthStencilRenderTarget.DepthStencilTarget->GetDesc().NumSamples);
        // CustomResolveSubpass can have targets with a different NumSamples
        ensure(!NumSamples || CurrDesc.samples == NumSamples || RPInfo.SubpassHint == SubpassHint::CustomResolveSubpass);
        NumSamples = CurrDesc.samples;
        CurrDesc.format = UEToVkTextureFormat(RPInfo.DepthStencilRenderTarget.DepthStencilTarget->GetDesc().Format, false);
        CurrDesc.loadOp = RenderTargetLoadActionToVulkan(GetLoadAction(GetDepthActions(RPInfo.DepthStencilRenderTarget.Action)));
        CurrDesc.stencilLoadOp = RenderTargetLoadActionToVulkan(GetLoadAction(GetStencilActions(RPInfo.DepthStencilRenderTarget.Action)));
        bFoundClearOp = bFoundClearOp || (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
        if (CurrDesc.samples != VK_SAMPLE_COUNT_1_BIT)
        {
            // Can't resolve MSAA depth/stencil
            ensure(GetStoreAction(GetDepthActions(RPInfo.DepthStencilRenderTarget.Action)) != RenderTargetStoreAction::EMultisampleResolve);
            ensure(GetStoreAction(GetStencilActions(RPInfo.DepthStencilRenderTarget.Action)) != RenderTargetStoreAction::EMultisampleResolve);
        }

        CurrDesc.storeOp = RenderTargetStoreActionToVulkan(GetStoreAction(GetDepthActions(RPInfo.DepthStencilRenderTarget.Action)));
        CurrDesc.stencilStoreOp = RenderTargetStoreActionToVulkan(GetStoreAction(GetStencilActions(RPInfo.DepthStencilRenderTarget.Action)));

        if (EnumHasAnyFlags(TextureDesc.Flags, TexCreate_Memoryless))
        {
            ensure(CurrDesc.storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
            ensure(CurrDesc.stencilStoreOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
        }

        ExclusiveDepthStencil ExclusiveDepthStencil = RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil;
        if (Platform::RequiresDepthWriteOnStencilClear() &&
            RPInfo.DepthStencilRenderTarget.Action == DepthStencilTargetActions::LoadDepthClearStencil_StoreDepthStencil)
        {
            ExclusiveDepthStencil = ExclusiveDepthStencil::DepthWrite_StencilWrite;
            CurrentDepthLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
            CurrentStencilLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        }

        // If the initial != final we need to change the FullHashInfo and use FinalLayout
        CurrDesc.initialLayout = CurrentDepthLayout;
        CurrDesc.finalLayout = CurrentDepthLayout;
        StencilDesc.stencilInitialLayout = CurrentStencilLayout;
        StencilDesc.stencilFinalLayout = CurrentStencilLayout;

        // We can't have the final layout be UNDEFINED, but it's possible that we get here from a transient texture
        // where the stencil was never used yet.  We can set the layout to whatever we want, the next transition will
        // happen from UNDEFINED anyhow.
        if (CurrentDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            check(CurrDesc.storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
            CurrDesc.finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        }
        if (CurrentStencilLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            check(CurrDesc.stencilStoreOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
            StencilDesc.stencilFinalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        }

        DepthReference.attachment = NumAttachmentDescriptions;
        DepthReference.layout = CurrentDepthLayout;
        StencilReference.stencilLayout = CurrentStencilLayout;

        ++NumAttachmentDescriptions;

        FullHashInfo.LoadOps[MaxSimultaneousRenderTargets] = CurrDesc.loadOp;
        FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilLoadOp;
        FullHashInfo.StoreOps[MaxSimultaneousRenderTargets] = CurrDesc.storeOp;
        FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilStoreOp;
        FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets] = CurrentDepthLayout;
        FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets + 1] = CurrentStencilLayout;
        CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets] = CurrDesc.format;

        bHasDepthStencil = true;

        if (bSetExtent)
        {
            // Depth can be greater or equal to color. Clamp to the smaller size.
            Extent.Extent3D.width = std::min<uint32>(Extent.Extent3D.width, TextureDesc.Extent.x);
            Extent.Extent3D.height = std::min<uint32>(Extent.Extent3D.height, TextureDesc.Extent.x);
        }
        else
        {
            bSetExtent = true;
            Extent.Extent3D.width = TextureDesc.Extent.x;
            Extent.Extent3D.height = TextureDesc.Extent.y;
            Extent.Extent3D.depth = TextureDesc.Depth;
        }
    }
    else if (NumColorRenderTargets == 0)
    {
        // No Depth and no color, it's a raster-only pass so make sure the renderArea will be set up properly
        // checkf(RPInfo.ResolveRect.IsValid(), TEXT("For raster-only passes without render targets, ResolveRect has to contain the render area"));
        check(RPInfo.ResolveRect.IsValid());
        bSetExtent = true;
        Offset.Offset3D.x = RPInfo.ResolveRect.X1;
        Offset.Offset3D.y = RPInfo.ResolveRect.Y1;
        Offset.Offset3D.z = 0;
        Extent.Extent3D.width = RPInfo.ResolveRect.X2 - RPInfo.ResolveRect.X1;
        Extent.Extent3D.height = RPInfo.ResolveRect.Y2 - RPInfo.ResolveRect.Y1;
        Extent.Extent3D.depth = 1;
    }

    if (false /*GRHISupportsAttachmentVariableRateShading && GRHIVariableRateShadingEnabled && GRHIAttachmentVariableRateShadingEnabled && RPInfo.ShadingRateTexture*/)
    {
        printf("ERROR: Don't support VRS! %s %d\n", __FILE__, __LINE__);
    }

    SubpassHint = RPInfo.SubpassHint;
    CompatibleHashInfo.SubpassHint = (uint8)RPInfo.SubpassHint;

    CompatibleHashInfo.QCOMRenderPassTransform = QCOMRenderPassTransform;

    CompatibleHashInfo.NumSamples = NumSamples;
    CompatibleHashInfo.MultiViewCount = MultiViewCount;

    if (MultiViewCount > 1 && !bMultiviewRenderTargets)
    {
        // UE_LOG(LogVulkan, Error, TEXT("Non multiview textures on a multiview layout!"));
        printf("ERROR: Non multiview textures on a multiview layout! %s %d\n", __FILE__, __LINE__);
    }

    RenderPassCompatibleHash = MemCrc32(&CompatibleHashInfo, sizeof(CompatibleHashInfo), 0);
    RenderPassFullHash = MemCrc32(&FullHashInfo, sizeof(FullHashInfo), RenderPassCompatibleHash);
    NumUsedClearValues = bFoundClearOp ? NumAttachmentDescriptions : 0;
    bCalculatedHash = true;
}

// 582
void CommandListContext::BeginRenderPass(const RenderPassInfo &InInfo, const char *InName)
{
    CmdBuffer *CmdBuffer = commandBufferManager->GetActiveCmdBuffer();

    if (SafePointSubmit())
    {
        CmdBuffer = commandBufferManager->GetActiveCmdBuffer();
    }

    renderPassInfo = InInfo;
    // RHIPushEvent(InName ? InName : TEXT("<unnamed RenderPass>"), FColor::Green);
    if (InInfo.NumOcclusionQueries > 0)
    {
        assert(0);
        // BeginOcclusionQueryBatch(CmdBuffer, InInfo.NumOcclusionQueries);
    }

    Texture *DSTexture = InInfo.DepthStencilRenderTarget.DepthStencilTarget;
    VkImageLayout CurrentDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout CurrentStencilLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (DSTexture)
    {
        VulkanTexture &vulkanTexture = *dynamic_cast<VulkanTexture *>(DSTexture);
        const VkImageAspectFlags AspectFlags = vulkanTexture.GetFullAspectMask();

        if (GetDevice()->SupportsParallelRendering())
        {
            printf("ERROR %s %d\n", __FILE__, __LINE__);
            exit(-1);
            // const FExclusiveDepthStencil ExclusiveDepthStencil = InInfo.DepthStencilRenderTarget.ExclusiveDepthStencil;
            // if (VKHasAnyFlags(AspectFlags, VK_IMAGE_ASPECT_DEPTH_BIT))
            // {
            //     if (ExclusiveDepthStencil.IsDepthWrite())
            //     {
            //         CurrentDepthLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
            //     }
            //     else
            //     {
            //         // todo-jn: temporarily use tracking illegally because sometimes depth is left in ATTACHMENT_OPTIMAL for Read passes
            //         CurrentDepthLayout = CmdBuffer->GetLayoutManager().GetDepthStencilHint(VulkanTexture, VK_IMAGE_ASPECT_DEPTH_BIT);
            //         if (CurrentDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED)
            //         {
            //             CurrentDepthLayout = GetQueue()->GetLayoutManager().GetDepthStencilHint(VulkanTexture, VK_IMAGE_ASPECT_DEPTH_BIT);
            //             if (CurrentDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED)
            //             {
            //                 if (ExclusiveDepthStencil.IsDepthRead())
            //                 {
            //                     CurrentDepthLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
            //                 }
            //                 else
            //                 {
            //                     UE_LOG(LogVulkanRHI, Warning, TEXT("Render pass [%s] used an unknown depth state!"), InName ? InName : TEXT("unknown"));
            //                 }
            //             }
            //         }
            //     }
            // }

            // if (VKHasAnyFlags(AspectFlags, VK_IMAGE_ASPECT_STENCIL_BIT))
            // {
            //     if (ExclusiveDepthStencil.IsStencilWrite())
            //     {
            //         CurrentStencilLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
            //     }
            //     else if (ExclusiveDepthStencil.IsStencilRead())
            //     {
            //         CurrentStencilLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
            //     }
            //     else
            //     {
            //         // todo-jn: temporarily use tracking illegally because stencil is no-op so we can't know expected layout
            //         CurrentStencilLayout = CmdBuffer->GetLayoutManager().GetDepthStencilHint(VulkanTexture, VK_IMAGE_ASPECT_STENCIL_BIT);
            //         if (CurrentStencilLayout == VK_IMAGE_LAYOUT_UNDEFINED)
            //         {
            //             CurrentStencilLayout = GetQueue()->GetLayoutManager().GetDepthStencilHint(VulkanTexture, VK_IMAGE_ASPECT_STENCIL_BIT);
            //             // If the layout is still UNDEFINED, FVulkanRenderTargetLayout will force it into a known state
            //         }
            //     }
            // }
        }
        else
        {
            const ImageLayout *FullLayout = CmdBuffer->GetLayoutManager().GetFullLayout(vulkanTexture);
            check(FullLayout);
            if (VKHasAnyFlags(AspectFlags, VK_IMAGE_ASPECT_DEPTH_BIT))
            {
                CurrentDepthLayout = FullLayout->GetSubresLayout(0, 0, VK_IMAGE_ASPECT_DEPTH_BIT);
            }
            if (VKHasAnyFlags(AspectFlags, VK_IMAGE_ASPECT_STENCIL_BIT))
            {
                CurrentStencilLayout = FullLayout->GetSubresLayout(0, 0, VK_IMAGE_ASPECT_STENCIL_BIT);
            }
        }
    }

    RenderTargetLayout RTLayout(*device, InInfo, CurrentDepthLayout, CurrentStencilLayout);
    check(RTLayout.GetExtent2D().width != 0 && RTLayout.GetExtent2D().height != 0);

    RenderPass *RenderPass = device->GetRenderPassManager().GetOrCreateRenderPass(RTLayout);
    SetRenderTargetsInfo RTInfo;
    InInfo.ConvertToRenderTargetsInfo(RTInfo);

    Framebuffer *Framebuffer = device->GetRenderPassManager().GetOrCreateFramebuffer(RTInfo, RTLayout, RenderPass);
    check(RenderPass != nullptr && Framebuffer != nullptr);
    device->GetRenderPassManager().BeginRenderPass(*this, *device, CmdBuffer, InInfo, RTLayout, RenderPass, Framebuffer);

    check(!CurrentRenderPass);
    CurrentRenderPass = RenderPass;
    CurrentFramebuffer = Framebuffer;
}

void CommandListContext::EndRenderPass()
{
    const bool bHasOcclusionQueries = (renderPassInfo.NumOcclusionQueries > 0);
    CmdBuffer *CmdBuffer = commandBufferManager->GetActiveCmdBuffer();

    if (bHasOcclusionQueries)
    {
        // EndOcclusionQueryBatch(CmdBuffer);
    }

    device->GetRenderPassManager().EndRenderPass(CmdBuffer);

    check(CurrentRenderPass);
    CurrentRenderPass = nullptr;

    RHIPopEvent();

    // Sync point for passes with occlusion queries
    if (bHasOcclusionQueries && GSubmitOcclusionBatchCmdBufferCVar)
    {
        RequestSubmitCurrentCommands();
        SafePointSubmit();
    }
}

void CommandListContext::NextSubpass()
{
    check(CurrentRenderPass);
    CmdBuffer *cb = commandBufferManager->GetActiveCmdBuffer();
    vkCmdNextSubpass(cb->GetHandle(), VK_SUBPASS_CONTENTS_INLINE);
}

RenderTargetLayout::RenderTargetLayout(const GraphicsPipelineStateInitializer& Initializer)
	: NumAttachmentDescriptions(0)
	, NumColorAttachments(0)
	, bHasDepthStencil(false)
	, bHasResolveAttachments(false)
	, bHasFragmentDensityAttachment(false)
	, NumSamples(0)
	, NumUsedClearValues(0)
	, MultiViewCount(0)
{
	ResetAttachments();

	RenderPassCompatibleHashableStruct CompatibleHashInfo;
	RenderPassFullHashableStruct FullHashInfo;

	bool bFoundClearOp = false;
	MultiViewCount = Initializer.MultiViewCount;
	NumSamples = Initializer.NumSamples;
	for (uint32 Index = 0; Index < Initializer.RenderTargetsEnabled; ++Index)
	{
		PixelFormat UEFormat = (PixelFormat)Initializer.RenderTargetFormats[Index];
		if (UEFormat != PF_Unknown)
		{
			// With a CustomResolveSubpass last color attachment is a resolve target
			bool bCustomResolveAttachment = (Index == (Initializer.RenderTargetsEnabled - 1)) && Initializer.SubpassHint == SubpassHint::CustomResolveSubpass;
			
			VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
			CurrDesc.samples = bCustomResolveAttachment ? VK_SAMPLE_COUNT_1_BIT : static_cast<VkSampleCountFlagBits>(NumSamples);
			CurrDesc.format = UEToVkTextureFormat(UEFormat, EnumHasAllFlags(Initializer.RenderTargetFlags[Index], TexCreate_SRGB));
			CurrDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			// If the initial != final we need to change the FullHashInfo and use FinalLayout
			CurrDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			CurrDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			ColorReferences[NumColorAttachments].attachment = NumAttachmentDescriptions;
			ColorReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT)
			{
				Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
				Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
				Desc[NumAttachmentDescriptions + 1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				Desc[NumAttachmentDescriptions + 1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				ResolveReferences[NumColorAttachments].attachment = NumAttachmentDescriptions + 1;
				ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				CompatibleHashInfo.AttachmentsToResolve |= (uint16)(1 << NumColorAttachments);
				++NumAttachmentDescriptions;
				bHasResolveAttachments = true;
			}

			CompatibleHashInfo.Formats[NumColorAttachments] = CurrDesc.format;
			FullHashInfo.LoadOps[NumColorAttachments] = CurrDesc.loadOp;
			FullHashInfo.StoreOps[NumColorAttachments] = CurrDesc.storeOp;
			FullHashInfo.InitialLayout[NumColorAttachments] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			++CompatibleHashInfo.NumAttachments;

			++NumAttachmentDescriptions;
			++NumColorAttachments;
		}
	}

	if (Initializer.DepthStencilTargetFormat != PF_Unknown)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		CurrDesc={};

		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
		CurrDesc.format = UEToVkTextureFormat(Initializer.DepthStencilTargetFormat, false);
		CurrDesc.loadOp = RenderTargetLoadActionToVulkan(Initializer.DepthTargetLoadAction);
		CurrDesc.stencilLoadOp = RenderTargetLoadActionToVulkan(Initializer.StencilTargetLoadAction);
		if (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
		{
			bFoundClearOp = true;
		}
		if (CurrDesc.samples == VK_SAMPLE_COUNT_1_BIT)
		{
			CurrDesc.storeOp = RenderTargetStoreActionToVulkan(Initializer.DepthTargetStoreAction);
			CurrDesc.stencilStoreOp = RenderTargetStoreActionToVulkan(Initializer.StencilTargetStoreAction);
		}
		else
		{
			// Never want to store MSAA depth/stencil
			CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}

		// If the initial != final we need to change the FullHashInfo and use FinalLayout
		CurrDesc.initialLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		CurrDesc.finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		StencilDesc.stencilInitialLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		StencilDesc.stencilFinalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

		DepthReference.attachment = NumAttachmentDescriptions;
		DepthReference.layout = Initializer.DepthStencilAccess.IsDepthWrite() ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
		StencilReference.stencilLayout = Initializer.DepthStencilAccess.IsStencilWrite() ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets] = CurrDesc.loadOp;
		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets] = CurrDesc.storeOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilStoreOp;
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets] = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets + 1] = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets] = CurrDesc.format;

		++NumAttachmentDescriptions;
		bHasDepthStencil = true;
	}

	if (Initializer.bHasFragmentDensityAttachment)
	{
        printf("ERROR: Don't support Fragment Density Attachment %s %d\n",__FILE__,__LINE__);
        exit(-1);
		// VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		// CurrDesc={};

		// const VkImageLayout VRSLayout = GetVRSImageLayout();

		// check(GRHIVariableRateShadingImageFormat != PF_Unknown);

		// CurrDesc.flags = 0;
		// CurrDesc.format = UEToVkTextureFormat(GRHIVariableRateShadingImageFormat, false);
		// CurrDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		// CurrDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		// CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		// CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		// CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		// CurrDesc.initialLayout = VRSLayout;
		// CurrDesc.finalLayout = VRSLayout;

		// FragmentDensityReference.attachment = NumAttachmentDescriptions;
		// FragmentDensityReference.layout = VRSLayout;

		// FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilLoadOp;
		// FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilStoreOp;
		// FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets + 2] = VRSLayout;
		// CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets + 1] = CurrDesc.format;

		// ++NumAttachmentDescriptions;
		// bHasFragmentDensityAttachment = true;
	}

	SubpassHint = Initializer.SubpassHint;
	CompatibleHashInfo.SubpassHint = (uint8)Initializer.SubpassHint;

	CommandListContext& ImmediateContext = RHI::Get().GetDevice()->GetImmediateContext();

	if (RHI::Get().GetDevice()->GetOptionalExtensions().HasQcomRenderPassTransform)
	{
        printf("ERROR: Don't support Qcom Render Pass Transform %s %d\n",__FILE__,__LINE__);
        exit(-1);
		// VkFormat SwapchainImageFormat = ImmediateContext.GetSwapchainImageFormat();
		// if (Desc[0].format == SwapchainImageFormat)
		// {
		// 	// Potential Swapchain RenderPass
		// 	QCOMRenderPassTransform = ImmediateContext.GetSwapchainQCOMRenderPassTransform();
		// }
		// // TODO: add some checks to detect potential Swapchain pass
		// else if (SwapchainImageFormat == VK_FORMAT_UNDEFINED)
		// {
		// 	// WA: to have compatible RP created with VK_RENDER_PASS_CREATE_TRANSFORM_BIT_QCOM flag
		// 	QCOMRenderPassTransform = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
		// }
	}

	CompatibleHashInfo.QCOMRenderPassTransform = QCOMRenderPassTransform;

	CompatibleHashInfo.NumSamples = NumSamples;
	CompatibleHashInfo.MultiViewCount = MultiViewCount;

	RenderPassCompatibleHash = MemCrc32(&CompatibleHashInfo, sizeof(CompatibleHashInfo));
	RenderPassFullHash = MemCrc32(&FullHashInfo, sizeof(FullHashInfo), RenderPassCompatibleHash);
	NumUsedClearValues = bFoundClearOp ? NumAttachmentDescriptions : 0;
	bCalculatedHash = true;
}
