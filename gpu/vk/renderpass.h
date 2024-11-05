#pragma once
#include "gpu/RHI/RHIResources.h"
#include "gpu/core/assertion_macros.h"
#include "private.h"
#include "util.h"
#include "device.h"
#include "private.h"
#include "configuration.h"
#include "vulkan_memory.h"
#include <unordered_map>

class RenderPass;
class Framebuffer;
class CommandListContext;
extern int32 GVulkanInputAttachmentShaderRead;

template <typename TAttachmentReferenceType>
struct AttachmentReference : public TAttachmentReferenceType
{
    AttachmentReference() { ZeroStruct(); }

    AttachmentReference(const VkAttachmentReference &AttachmentReferenceIn, VkImageAspectFlags AspectMask)
    {
        SetAttachment(AttachmentReferenceIn, AspectMask);
    }

    inline void SetAttachment(const VkAttachmentReference &AttachmentReferenceIn, VkImageAspectFlags AspectMask) { checkNoEntry(); }
    inline void SetAttachment(const AttachmentReference<TAttachmentReferenceType> &AttachmentReferenceIn, VkImageAspectFlags AspectMask) { *this = AttachmentReferenceIn; }
    inline void SetDepthStencilAttachment(const VkAttachmentReference &AttachmentReferenceIn, const VkAttachmentReferenceStencilLayout *StencilReference, VkImageAspectFlags AspectMask, bool bSupportsParallelRendering) { checkNoEntry(); }
    inline void ZeroStruct() {}
    inline void SetAspect(uint32 Aspect) {}
};

template <>
inline void AttachmentReference<VkAttachmentReference>::SetAttachment(const VkAttachmentReference &AttachmentReferenceIn,
                                                                      VkImageAspectFlags AspectMask)
{
    attachment = AttachmentReferenceIn.attachment;
    layout = AttachmentReferenceIn.layout;
}

/*********************************************************/
template <typename TSubpassDescriptionType>
class SubpassDescription
{
};

template <>
struct SubpassDescription<VkSubpassDescription>
    : public VkSubpassDescription
{
    SubpassDescription()
    {
        memset(this, 0, sizeof(VkSubpassDescription));
        pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    }

    void SetColorAttachments(const std::vector<AttachmentReference<VkAttachmentReference>> &ColorAttachmentReferences, int OverrideCount = -1)
    {
        colorAttachmentCount = (OverrideCount == -1) ? ColorAttachmentReferences.size() : OverrideCount;
        pColorAttachments = ColorAttachmentReferences.data();
    }

    void SetResolveAttachments(const std::vector<AttachmentReference<VkAttachmentReference>> &ResolveAttachmentReferences)
    {
        if (ResolveAttachmentReferences.size() > 0)
        {
            check(colorAttachmentCount == ResolveAttachmentReferences.size());
            pResolveAttachments = ResolveAttachmentReferences.data();
        }
    }

    void SetDepthStencilAttachment(AttachmentReference<VkAttachmentReference> *DepthStencilAttachmentReference)
    {
        pDepthStencilAttachment = static_cast<VkAttachmentReference *>(DepthStencilAttachmentReference);
    }

    void SetInputAttachments(AttachmentReference<VkAttachmentReference> *InputAttachmentReferences, uint32 NumInputAttachmentReferences)
    {
        pInputAttachments = static_cast<VkAttachmentReference *>(InputAttachmentReferences);
        inputAttachmentCount = NumInputAttachmentReferences;
    }

    void SetShadingRateAttachment(void * /* ShadingRateAttachmentInfo */)
    {
        // No-op without VK_KHR_create_renderpass2
    }

    void SetMultiViewMask(uint32_t Mask)
    {
        // No-op without VK_KHR_create_renderpass2
    }
};

/*********************************************************/

template <typename TSubpassDependencyType>
struct SubpassDependency : public TSubpassDependencyType
{
};

template <>
struct SubpassDependency<VkSubpassDependency> : public VkSubpassDependency
{
    SubpassDependency()
    {
        memset(this, 0, sizeof(VkSubpassDependency));
    }
};

/*********************************************************** */
template <typename TAttachmentDescriptionType>
struct AttachmentDescription
{
};

template <>
struct AttachmentDescription<VkAttachmentDescription>
    : public VkAttachmentDescription
{
    AttachmentDescription()
    {
        memset(this, 0, sizeof(VkAttachmentDescription));
    }

    AttachmentDescription(const VkAttachmentDescription &InDesc)
    {
        flags = InDesc.flags;
        format = InDesc.format;
        samples = InDesc.samples;
        loadOp = InDesc.loadOp;
        storeOp = InDesc.storeOp;
        stencilLoadOp = InDesc.stencilLoadOp;
        stencilStoreOp = InDesc.stencilStoreOp;
        initialLayout = InDesc.initialLayout;
        finalLayout = InDesc.finalLayout;
    }

    AttachmentDescription(const VkAttachmentDescription &InDesc, const VkAttachmentDescriptionStencilLayout *InStencilDesc, bool bSupportsParallelRendering)
    {
        flags = InDesc.flags;
        format = InDesc.format;
        samples = InDesc.samples;
        loadOp = InDesc.loadOp;
        storeOp = InDesc.storeOp;
        stencilLoadOp = InDesc.stencilLoadOp;
        stencilStoreOp = InDesc.stencilStoreOp;

        const bool bHasStencilLayout = VulkanRHI::VulkanFormatHasStencil(InDesc.format) && (InStencilDesc != nullptr);
        const VkImageLayout StencilInitialLayout = bHasStencilLayout ? InStencilDesc->stencilInitialLayout : VK_IMAGE_LAYOUT_UNDEFINED;
        initialLayout = GetMergedDepthStencilLayout(InDesc.initialLayout, StencilInitialLayout);
        const VkImageLayout StencilFinalLayout = bHasStencilLayout ? InStencilDesc->stencilFinalLayout : VK_IMAGE_LAYOUT_UNDEFINED;
        finalLayout = GetMergedDepthStencilLayout(InDesc.finalLayout, StencilFinalLayout);
    }
};
/************************************************* */

template <typename T>
struct RenderPassCreateInfo
{
};

template <>
struct RenderPassCreateInfo<VkRenderPassCreateInfo> : public VkRenderPassCreateInfo
{
    RenderPassCreateInfo() { ZeroVulkanStruct(*this, VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO); }

    void SetCorrelationMask(const uint32_t *MaskPtr) { /* No-op without VK_KHR_create_renderpass2*/ }

    VkRenderPass Create(Device &Device)
    {
        VkRenderPass Handle = VK_NULL_HANDLE;
        VERIFYVULKANRESULT_EXPANDED(vkCreateRenderPass(Device.GetInstanceHandle(), this, VULKAN_CPU_ALLOCATOR, &Handle));
        return Handle;
    }
};

// template <typename TSubpassDescriptionClass, typename TSubpassDependencyClass,
//           typename TAttachmentReferenceClass, typename TAttachmentDescriptionClass,
//           typename TRenderPassCreateInfoClass>

using TSubpassDescriptionClass = SubpassDescription<VkSubpassDescription>;
using TSubpassDependencyClass = SubpassDependency<VkSubpassDependency>;
using TAttachmentReferenceClass = AttachmentReference<VkAttachmentReference>;
using TAttachmentDescriptionClass = AttachmentDescription<VkAttachmentDescription>;
using TRenderPassCreateInfoClass = RenderPassCreateInfo<VkRenderPassCreateInfo>;
class RenderPassBuilder
{
public:
    RenderPassBuilder(Device &InDevice) : device(InDevice), CorrelationMask(0) {}

    void BuildCreateInfo(const RenderTargetLayout &RTLayout)
    {
        uint32 NumSubpasses = 0;
        uint32 NumDependencies = 0;

        // 0b11 for 2, 0b1111 for 4, and so on
        uint32 MultiviewMask = (0b1 << RTLayout.GetMultiViewCount()) - 1;

        const bool bDeferredShadingSubpass = RTLayout.GetSubpassHint() == SubpassHint::DeferredShadingSubpass;
        const bool bApplyFragmentShadingRate = false;
        const bool bCustomResolveSubpass = RTLayout.GetSubpassHint() == SubpassHint::CustomResolveSubpass;
        const bool bDepthReadSubpass = bCustomResolveSubpass || (RTLayout.GetSubpassHint() == SubpassHint::DepthReadSubpass);
        const bool bHasDepthStencilAttachmentReference = (RTLayout.GetDepthAttachmentReference() != nullptr);

        if (bApplyFragmentShadingRate)
        {
            // ShadingRateAttachmentReference.SetAttachment(*RTLayout.GetFragmentDensityAttachmentReference(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);
            // FragmentShadingRateAttachmentInfo.SetReference(&ShadingRateAttachmentReference);
        }

        // Grab (and optionally convert) attachment references.
        uint32 NumColorAttachments = RTLayout.GetNumColorAttachments();
        for (uint32 ColorAttachment = 0; ColorAttachment < NumColorAttachments; ++ColorAttachment)
        {
            ColorAttachmentReferences.push_back(TAttachmentReferenceClass(RTLayout.GetColorAttachmentReferences()[ColorAttachment], 0));
            if (RTLayout.GetResolveAttachmentReferences() != nullptr)
            {
                ResolveAttachmentReferences.push_back(TAttachmentReferenceClass(RTLayout.GetResolveAttachmentReferences()[ColorAttachment], 0));
            }
        }

        // CustomResolveSubpass has an additional color attachment that should not be used by main and depth subpasses
        if (bCustomResolveSubpass && (NumColorAttachments > 1))
        {
            NumColorAttachments--;
        }

        uint32_t DepthInputAttachment = VK_ATTACHMENT_UNUSED;
        VkImageLayout DepthInputAttachmentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageAspectFlags DepthInputAspectMask = 0;
        if (bHasDepthStencilAttachmentReference)
        {
            DepthStencilAttachmentReference.SetDepthStencilAttachment(
                *RTLayout.GetDepthAttachmentReference(),
                RTLayout.GetStencilAttachmentReference(),
                0,
                device.SupportsParallelRendering());

            if (bDepthReadSubpass || bDeferredShadingSubpass)
            {
                DepthStencilAttachment.attachment = RTLayout.GetDepthAttachmentReference()->attachment;
                DepthStencilAttachment.SetAspect(VK_IMAGE_ASPECT_DEPTH_BIT); // @todo?

                // FIXME: checking a Depth layout is not correct in all cases
                // PSO cache can create a PSO for subpass 1 or 2 first, where depth is read-only but that does not mean depth pre-pass is enabled
                if (false && RTLayout.GetDepthAttachmentReference()->layout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)
                {
                    // Depth is read only and is expected to be sampled as a regular texture
                    DepthStencilAttachment.layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
                }
                else
                {
                    // lights write to stencil for culling, so stencil is expected to be writebale while depth is read-only
                    DepthStencilAttachment.layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
                    DepthInputAttachment = DepthStencilAttachment.attachment;
                    DepthInputAttachmentLayout = DepthStencilAttachment.layout;
                    DepthInputAspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                }
            }
        }

        // main sub-pass
        {
            TSubpassDescriptionClass &SubpassDesc = SubpassDescriptions[NumSubpasses++];

            SubpassDesc.SetColorAttachments(ColorAttachmentReferences, NumColorAttachments);

            if (bHasDepthStencilAttachmentReference)
            {
                SubpassDesc.SetDepthStencilAttachment(&DepthStencilAttachmentReference);
            }

            if (bApplyFragmentShadingRate)
            {
                // SubpassDesc.SetShadingRateAttachment(&FragmentShadingRateAttachmentInfo);
            }
            SubpassDesc.SetMultiViewMask(MultiviewMask);
        }

        // Color write and depth read sub-pass
        if (bDepthReadSubpass)
        {
            TSubpassDescriptionClass &SubpassDesc = SubpassDescriptions[NumSubpasses++];

            SubpassDesc.SetColorAttachments(ColorAttachmentReferences, 1);

            check(RTLayout.GetDepthAttachmentReference());

            // Depth as Input0
            InputAttachments1[0].attachment = DepthInputAttachment;
            InputAttachments1[0].layout = DepthInputAttachmentLayout;
            InputAttachments1[0].SetAspect(DepthInputAspectMask);
            SubpassDesc.SetInputAttachments(InputAttachments1, InputAttachment1Count);
            // depth attachment is same as input attachment
            SubpassDesc.SetDepthStencilAttachment(&DepthStencilAttachment);

            if (bApplyFragmentShadingRate)
            {
                // SubpassDesc.SetShadingRateAttachment(&FragmentShadingRateAttachmentInfo);
            }
            SubpassDesc.SetMultiViewMask(MultiviewMask);

            TSubpassDependencyClass &SubpassDep = SubpassDependencies[NumDependencies++];
            SubpassDep.srcSubpass = 0;
            SubpassDep.dstSubpass = 1;
            SubpassDep.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            SubpassDep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            SubpassDep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            SubpassDep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            SubpassDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        }

        // Two subpasses for deferred shading
        if (bDeferredShadingSubpass)
        {
            // const VkAttachmentReference* ColorRef = RTLayout.GetColorAttachmentReferences();
            // uint32 NumColorAttachments = RTLayout.GetNumColorAttachments();
            // check(RTLayout.GetNumColorAttachments() == 5); //current layout is SceneColor, GBufferA/B/C/D

            // 1. Write to SceneColor and GBuffer, input DepthStencil
            {
                TSubpassDescriptionClass &SubpassDesc = SubpassDescriptions[NumSubpasses++];
                SubpassDesc.SetColorAttachments(ColorAttachmentReferences);
                SubpassDesc.SetDepthStencilAttachment(&DepthStencilAttachment);
                InputAttachments1[0].attachment = DepthInputAttachment;
                InputAttachments1[0].layout = DepthInputAttachmentLayout;
                InputAttachments1[0].SetAspect(DepthInputAspectMask);
                SubpassDesc.SetInputAttachments(InputAttachments1, InputAttachment1Count);

                if (bApplyFragmentShadingRate)
                {
                    // SubpassDesc.SetShadingRateAttachment(&FragmentShadingRateAttachmentInfo);
                }
                SubpassDesc.SetMultiViewMask(MultiviewMask);

                // Depth as Input0
                TSubpassDependencyClass &SubpassDep = SubpassDependencies[NumDependencies++];
                SubpassDep.srcSubpass = 0;
                SubpassDep.dstSubpass = 1;
                SubpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                SubpassDep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                SubpassDep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                SubpassDep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
                SubpassDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            }

            // 2. Write to SceneColor, input GBuffer and DepthStencil
            {
                TSubpassDescriptionClass &SubpassDesc = SubpassDescriptions[NumSubpasses++];
                SubpassDesc.SetColorAttachments(ColorAttachmentReferences, 1); // SceneColor only
                SubpassDesc.SetDepthStencilAttachment(&DepthStencilAttachment);

                // Depth as Input0
                InputAttachments2[0].attachment = DepthInputAttachment;
                InputAttachments2[0].layout = DepthInputAttachmentLayout;
                InputAttachments2[0].SetAspect(DepthInputAspectMask);

                // SceneColor write only
                InputAttachments2[1].attachment = VK_ATTACHMENT_UNUSED;
                InputAttachments2[1].layout = VK_IMAGE_LAYOUT_UNDEFINED;
                InputAttachments2[1].SetAspect(0);

                // GBufferA/B/C/D as Input2/3/4/5
                int32 NumColorInputs = ColorAttachmentReferences.size() - 1;
                for (int32 i = 2; i < (NumColorInputs + 2); ++i)
                {
                    InputAttachments2[i].attachment = ColorAttachmentReferences[i - 1].attachment;
                    InputAttachments2[i].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    InputAttachments2[i].SetAspect(VK_IMAGE_ASPECT_COLOR_BIT);
                }

                SubpassDesc.SetInputAttachments(InputAttachments2, NumColorInputs + 2);
                if (bApplyFragmentShadingRate)
                {
                    // SubpassDesc.SetShadingRateAttachment(&FragmentShadingRateAttachmentInfo);
                }
                SubpassDesc.SetMultiViewMask(MultiviewMask);

                TSubpassDependencyClass &SubpassDep = SubpassDependencies[NumDependencies++];
                SubpassDep.srcSubpass = 1;
                SubpassDep.dstSubpass = 2;
                SubpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                SubpassDep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                SubpassDep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                SubpassDep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
                if (GVulkanInputAttachmentShaderRead == 1)
                {
                    // this is not required, but might flicker on some devices without
                    SubpassDep.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                }
                SubpassDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            }
        }

        // Custom resolve subpass
        if (bCustomResolveSubpass)
        {
            TSubpassDescriptionClass &SubpassDesc = SubpassDescriptions[NumSubpasses++];
            ColorAttachments3[0].attachment = ColorAttachmentReferences[1].attachment;
            ColorAttachments3[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            ColorAttachments3[0].SetAspect(VK_IMAGE_ASPECT_COLOR_BIT);

            InputAttachments3[0].attachment = VK_ATTACHMENT_UNUSED; // The subpass fetch logic expects depth in first attachment.
            InputAttachments3[0].layout = VK_IMAGE_LAYOUT_UNDEFINED;
            InputAttachments3[0].SetAspect(0);

            InputAttachments3[1].attachment = ColorAttachmentReferences[0].attachment; // SceneColor as input
            InputAttachments3[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            InputAttachments3[1].SetAspect(VK_IMAGE_ASPECT_COLOR_BIT);

            SubpassDesc.SetInputAttachments(InputAttachments3, 2);
            SubpassDesc.colorAttachmentCount = 1;
            SubpassDesc.pColorAttachments = ColorAttachments3;

            SubpassDesc.SetMultiViewMask(MultiviewMask);

            TSubpassDependencyClass &SubpassDep = SubpassDependencies[NumDependencies++];
            SubpassDep.srcSubpass = 1;
            SubpassDep.dstSubpass = 2;
            SubpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            SubpassDep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            SubpassDep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            SubpassDep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            SubpassDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        }

        // CustomResolveSubpass does a custom resolve into second color target and does not need resolve attachments
        if (!bCustomResolveSubpass)
        {
            if (bDepthReadSubpass && ResolveAttachmentReferences.size() > 1)
            {
                // Handle SceneDepthAux resolve:
                // The depth read subpass has only a single color attachment (using more would not be compatible dual source blending), so make SceneDepthAux a resolve attachment of the first subpass instead
                ResolveAttachmentReferences.push_back(TAttachmentReferenceClass{});
                ResolveAttachmentReferences.back().attachment = VK_ATTACHMENT_UNUSED;
                std::swap(ResolveAttachmentReferences.back(), ResolveAttachmentReferences[0]);

                std::vector<TAttachmentReferenceClass> temp;
                for (int i = 0; i < ResolveAttachmentReferences.size() - 1; ++i)
                {
                    temp.push_back(ResolveAttachmentReferences[i]);
                }
                SubpassDescriptions[0].SetResolveAttachments(temp);
                temp.clear();
                temp.push_back(ResolveAttachmentReferences.back());
                SubpassDescriptions[NumSubpasses - 1].SetResolveAttachments(temp);
            }
            else
            {
                // Only set resolve attachment on the last subpass
                SubpassDescriptions[NumSubpasses - 1].SetResolveAttachments(ResolveAttachmentReferences);
            }
        }

        for (uint32 Attachment = 0; Attachment < RTLayout.GetNumAttachmentDescriptions(); ++Attachment)
        {
            if (bHasDepthStencilAttachmentReference && (Attachment == DepthStencilAttachmentReference.attachment))
            {
                AttachmentDescriptions.push_back(TAttachmentDescriptionClass(RTLayout.GetAttachmentDescriptions()[Attachment], RTLayout.GetStencilDesc(), device.SupportsParallelRendering()));
            }
            else
            {
                AttachmentDescriptions.push_back(TAttachmentDescriptionClass(RTLayout.GetAttachmentDescriptions()[Attachment]));
            }
        }

        CreateInfo.attachmentCount = AttachmentDescriptions.size();
        CreateInfo.pAttachments = AttachmentDescriptions.data();
        CreateInfo.subpassCount = NumSubpasses;
        CreateInfo.pSubpasses = SubpassDescriptions;
        CreateInfo.dependencyCount = NumDependencies;
        CreateInfo.pDependencies = SubpassDependencies;

        /*
        Bit mask that specifies which view rendering is broadcast to
        0011 = Broadcast to first and second view (layer)
        */
        const uint32_t ViewMask[2] = {MultiviewMask, MultiviewMask};

        /*
        Bit mask that specifices correlation between views
        An implementation may use this for optimizations (concurrent render)
        */
        CorrelationMask = MultiviewMask;

#ifdef PRINT_UMPLEMENT
        printf("Don't support MultiView and Fragment Density Map %s %d\n", __FILE__, __LINE__);
#endif
        /*
        if (RTLayout.GetIsMultiView())
        {
            if (device.GetOptionalExtensions().HasKHRRenderPass2)
            {
                CreateInfo.SetCorrelationMask(&CorrelationMask);
            }
            else
            {
                checkf(device.GetOptionalExtensions().HasKHRMultiview, TEXT("Layout is multiview but extension is not supported!"));
                MultiviewInfo.subpassCount = NumSubpasses;
                MultiviewInfo.pViewMasks = ViewMask;
                MultiviewInfo.dependencyCount = 0;
                MultiviewInfo.pViewOffsets = nullptr;
                MultiviewInfo.correlationMaskCount = 1;
                MultiviewInfo.pCorrelationMasks = &CorrelationMask;

                MultiviewInfo.pNext = CreateInfo.pNext;
                CreateInfo.pNext = &MultiviewInfo;
            }
        }

        if (device.GetOptionalExtensions().HasEXTFragmentDensityMap && RTLayout.GetHasFragmentDensityAttachment())
        {
            FragDensityCreateInfo.fragmentDensityMapAttachment = *RTLayout.GetFragmentDensityAttachmentReference();

            // Chain fragment density info onto create info and the rest of the pNexts
            // onto the fragment density info
            FragDensityCreateInfo.pNext = CreateInfo.pNext;
            CreateInfo.pNext = &FragDensityCreateInfo;
        }*/

#if VULKAN_SUPPORTS_QCOM_RENDERPASS_TRANSFORM
        if (RTLayout.GetQCOMRenderPassTransform() != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        {
            CreateInfo.flags = VK_RENDER_PASS_CREATE_TRANSFORM_BIT_QCOM;
        }
#endif
    }

    VkRenderPass Create(const RenderTargetLayout &RTLayout)
    {
        BuildCreateInfo(RTLayout);
        return CreateInfo.Create(device);
    }

private:
    TSubpassDescriptionClass SubpassDescriptions[8];
    TSubpassDependencyClass SubpassDependencies[8];

    std::vector<TAttachmentReferenceClass> ColorAttachmentReferences;
    std::vector<TAttachmentReferenceClass> ResolveAttachmentReferences;

    // Color write and depth read sub-pass
    static const uint32 InputAttachment1Count = 1;
    TAttachmentReferenceClass InputAttachments1[InputAttachment1Count];

    // Two subpasses for deferred shading
    TAttachmentReferenceClass InputAttachments2[MaxSimultaneousRenderTargets + 1];

    TAttachmentReferenceClass DepthStencilAttachment;
    TAttachmentReferenceClass DepthStencilAttachmentReference;

    std::vector<TAttachmentDescriptionClass> AttachmentDescriptions;

    // Tonemap subpass
    TAttachmentReferenceClass InputAttachments3[MaxSimultaneousRenderTargets + 1];
    TAttachmentReferenceClass ColorAttachments3[MaxSimultaneousRenderTargets + 1];

    TRenderPassCreateInfoClass CreateInfo;
    Device &device;
    uint32_t CorrelationMask;
};

VkRenderPass CreateVulkanRenderPass(Device &device, const RenderTargetLayout &RTLayout);

class RenderPassManager : public VulkanRHI::DeviceChild
{
public:
    RenderPassManager(Device *InDevice) : VulkanRHI::DeviceChild(InDevice) {}
    ~RenderPassManager();

    Framebuffer *GetOrCreateFramebuffer(const SetRenderTargetsInfo &RenderTargetsInfo,
                                        const RenderTargetLayout &RTLayout, RenderPass *RenderPass);

    RenderPass *GetOrCreateRenderPass(const RenderTargetLayout &RTLayout)
    {
        const uint32 RenderPassHash = RTLayout.GetRenderPassFullHash();

        {
            std::unordered_map<uint32, RenderPass *>::iterator FoundRenderPass = RenderPasses.find(RenderPassHash);
            if (FoundRenderPass != RenderPasses.end())
            {
                return FoundRenderPass->second;
            }
        }

        RenderPass *renderPass = new RenderPass(*device, RTLayout);
        {
            // FRWScopeLock ScopedWriteLock(RenderPassesLock, SLT_Write);
            auto FoundRenderPass = RenderPasses.find(RenderPassHash);
            if (FoundRenderPass != RenderPasses.end())
            {
                delete renderPass;
                return FoundRenderPass->second;
            }
            RenderPasses.insert(std::pair(RenderPassHash, renderPass));
        }
        return renderPass;
    }

    void BeginRenderPass(CommandListContext &Context, Device &InDevice, CmdBuffer *CmdBuffer,
                         const RenderPassInfo &RPInfo, const RenderTargetLayout &RTLayout, RenderPass *RenderPass,
                         Framebuffer *Framebuffer);
    void EndRenderPass(CmdBuffer *CmdBuffer);

    void NotifyDeletedRenderTarget(VkImage Image);

private:
    std::unordered_map<uint32, RenderPass *> RenderPasses;
    struct FramebufferList
    {
        std::vector<Framebuffer *> Framebuffer;
    };
    std::unordered_map<uint32, FramebufferList *> Framebuffers;
};