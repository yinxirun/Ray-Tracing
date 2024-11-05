#include "renderpass.h"
#include "barrier.h"
#include "command_buffer.h"
#include "gpu/core/misc/crc.h"
#include "gpu/math/color.h"
#include "context.h"
#include "pending_state.h"

VkRenderPass CreateVulkanRenderPass(Device &InDevice, const RenderTargetLayout &RTLayout)
{
    VkRenderPass OutRenderpass;

// if (InDevice.GetOptionalExtensions().HasKHRRenderPass2)
// {
// 	FVulkanRenderPassBuilder<FVulkanSubpassDescription<VkSubpassDescription2>, FVulkanSubpassDependency<VkSubpassDependency2>, FVulkanAttachmentReference<VkAttachmentReference2>, FVulkanAttachmentDescription<VkAttachmentDescription2>, FVulkanRenderPassCreateInfo<VkRenderPassCreateInfo2>> Creator(InDevice);
// 	OutRenderpass = Creator.Create(RTLayout);
// }
// else
#ifdef PRINT_UNIMPLEMENT
    printf("Don't support Render Pass 2 %s %d\n", __FILE__, __LINE__);
#endif
    {
        // RenderPassBuilder<FVulkanSubpassDescription<VkSubpassDescription>, FVulkanSubpassDependency<VkSubpassDependency>, FVulkanAttachmentReference<VkAttachmentReference>, FVulkanAttachmentDescription<VkAttachmentDescription>, FVulkanRenderPassCreateInfo<VkRenderPassCreateInfo>> Creator(InDevice);
        RenderPassBuilder Creator(InDevice);

        OutRenderpass = Creator.Create(RTLayout);
    }

    return OutRenderpass;
}

RenderPassManager::~RenderPassManager()
{
    // check(!GIsRHIInitialized);

    for (auto &Pair : RenderPasses)
    {
        delete Pair.second;
    }

    for (auto &Pair : Framebuffers)
    {
        FramebufferList *List = Pair.second;
        for (int32 Index = List->Framebuffer.size() - 1; Index >= 0; --Index)
        {
            List->Framebuffer[Index]->Destroy(*device);
            delete List->Framebuffer[Index];
        }
        delete List;
    }

    RenderPasses.clear();
    Framebuffers.clear();
}

/// @details 可以复用之前创建的framebuffer，之前的framebuffer存储在std::unordered_map<uint32, FramebufferList*>类型的成员中。
 /// 虽然一个VkFramebuffer和一个特定的VkRenderPass对象绑定（创建Framebuffer时需指定RenderPass），看似不能复用，但是根据Vulkan Documentation，\
只要一个Framebufer和一个RenderPass是兼容的，它们就可以一起使用，即使该Framebuffer不是使用此RenderPass创建的。
/// 关于“兼容”的定义，详情可见Vulkan Documentation中的Render Pass Compatibility的相关部分。
Framebuffer *RenderPassManager::GetOrCreateFramebuffer(const SetRenderTargetsInfo &RenderTargetsInfo,
                                                       const RenderTargetLayout &RTLayout, RenderPass *RenderPass)
{
    // todo-jn: threadsafe?

    uint32 RTLayoutHash = RTLayout.GetRenderPassCompatibleHash();

    uint64 MipsAndSlicesValues[MaxSimultaneousRenderTargets];
    for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
    {
        MipsAndSlicesValues[Index] = ((uint64)RenderTargetsInfo.ColorRenderTarget[Index].ArraySliceIndex << (uint64)32) | (uint64)RenderTargetsInfo.ColorRenderTarget[Index].MipIndex;
    }
    RTLayoutHash = MemCrc32(MipsAndSlicesValues, sizeof(MipsAndSlicesValues), RTLayoutHash);

    auto FindFramebufferInList = [&](FramebufferList *InFramebufferList)
    {
        Framebuffer *OutFramebuffer = nullptr;

        for (int32 Index = 0; Index < InFramebufferList->Framebuffer.size(); ++Index)
        {
            const VkRect2D RenderArea = InFramebufferList->Framebuffer[Index]->GetRenderArea();

            if (InFramebufferList->Framebuffer[Index]->Matches(RenderTargetsInfo) &&
                ((RTLayout.GetExtent2D().width == RenderArea.extent.width) && (RTLayout.GetExtent2D().height == RenderArea.extent.height) &&
                 (RTLayout.GetOffset2D().x == RenderArea.offset.x) && (RTLayout.GetOffset2D().y == RenderArea.offset.y)))
            {
                OutFramebuffer = InFramebufferList->Framebuffer[Index];
                break;
            }
        }

        return OutFramebuffer;
    };

    std::unordered_map<uint32, FramebufferList *>::iterator FoundFramebufferList;
    FramebufferList *framebufferList = nullptr;

    {
        // FRWScopeLock ScopedReadLock(FramebuffersLock, SLT_ReadOnly);

        FoundFramebufferList = Framebuffers.find(RTLayoutHash);
        if (FoundFramebufferList != Framebuffers.end())
        {
            framebufferList = FoundFramebufferList->second;

            Framebuffer *ExistingFramebuffer = FindFramebufferInList(framebufferList);
            if (ExistingFramebuffer)
            {
                return ExistingFramebuffer;
            }
        }
    }

    // FRWScopeLock ScopedWriteLock(FramebuffersLock, SLT_Write);
    FoundFramebufferList = Framebuffers.find(RTLayoutHash);
    if (FoundFramebufferList == Framebuffers.end())
    {
        framebufferList = new FramebufferList;
        Framebuffers.insert(std::pair(RTLayoutHash, framebufferList));
    }
    else
    {
        framebufferList = FoundFramebufferList->second;
        Framebuffer *ExistingFramebuffer = FindFramebufferInList(framebufferList);
        if (ExistingFramebuffer)
        {
            return ExistingFramebuffer;
        }
    }

    Framebuffer *framebuffer = new Framebuffer(*device, RenderTargetsInfo, RTLayout, *RenderPass);
    framebufferList->Framebuffer.push_back(framebuffer);
    return framebuffer;
}

void RenderPassManager::BeginRenderPass(CommandListContext &Context, Device &InDevice, CmdBuffer *CmdBuffer,
                                        const RenderPassInfo &RPInfo, const RenderTargetLayout &RTLayout,
                                        RenderPass *RenderPass, Framebuffer *Framebuffer)
{
    // (NumRT + 1 [Depth] ) * 2 [surface + resolve]
    VkClearValue ClearValues[(MaxSimultaneousRenderTargets + 1) * 2];
    uint32 ClearValueIndex = 0;
    bool bNeedsClearValues = RenderPass->GetNumUsedClearValues() > 0;
    memset(ClearValues, 0, sizeof(ClearValues));

    int32 NumColorTargets = RPInfo.GetNumColorRenderTargets();
    int32 Index = 0;

    PipelineBarrier Barrier;

    for (Index = 0; Index < NumColorTargets; ++Index)
    {
        const RenderPassInfo::ColorEntry &ColorEntry = RPInfo.ColorRenderTargets[Index];

        Texture *ColorTexture = ColorEntry.RenderTarget;

        VulkanTexture &ColorSurface = dynamic_cast<VulkanTexture &>(*ColorTexture);
        const bool bPassPerformsResolve = ColorSurface.GetDesc().NumSamples > 1 && ColorEntry.ResolveTarget;

        if (GetLoadAction(ColorEntry.Action) == RenderTargetLoadAction::ELoad)
        {
            // Insert a barrier if we're loading from any color targets, to make sure the passes aren't reordered and we end up running before
            // the pass we're supposed to read from.
            const VkAccessFlags AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            const VkPipelineStageFlags StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            Barrier.AddMemoryBarrier(AccessMask, AccessMask, StageMask, StageMask);
        }

        if (bNeedsClearValues)
        {
            const LinearColor &ClearColor = ColorTexture->HasClearValue() ? ColorTexture->GetClearColor() : LinearColor::Black;
            ClearValues[ClearValueIndex].color.float32[0] = ClearColor.R;
            ClearValues[ClearValueIndex].color.float32[1] = ClearColor.G;
            ClearValues[ClearValueIndex].color.float32[2] = ClearColor.B;
            ClearValues[ClearValueIndex].color.float32[3] = ClearColor.A;
            ++ClearValueIndex;
            if (bPassPerformsResolve)
            {
                ++ClearValueIndex;
            }
        }
    }

    Texture *DSTexture = RPInfo.DepthStencilRenderTarget.DepthStencilTarget;
    if (DSTexture)
    {
        const ExclusiveDepthStencil RequestedDSAccess = RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil;
        if (RequestedDSAccess.IsDepthRead() || RequestedDSAccess.IsStencilRead())
        {
            // If the depth-stencil state doesn't change between passes, the high level code won't perform any transitions.
            // Make sure we have a barrier in case we're loading depth or stencil, to prevent rearranging passes.
            const VkAccessFlags AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            const VkPipelineStageFlags StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            Barrier.AddMemoryBarrier(AccessMask, AccessMask, StageMask, StageMask);
        }

        if (DSTexture->HasClearValue() && bNeedsClearValues)
        {
            float Depth = 0;
            uint32 Stencil = 0;
            DSTexture->GetDepthStencilClearValue(Depth, Stencil);
            ClearValues[ClearValueIndex].depthStencil.depth = Depth;
            ClearValues[ClearValueIndex].depthStencil.stencil = Stencil;
            ++ClearValueIndex;
        }
    }

    // RHITexture *ShadingRateTexture = RPInfo.ShadingRateTexture;
    // if (ShadingRateTexture)
    // {
    //     ValidateShadingRateDataType();
    // }

    ensure(ClearValueIndex <= RenderPass->GetNumUsedClearValues());

    Barrier.Execute(CmdBuffer);

    CmdBuffer->BeginRenderPass(RenderPass->GetLayout(), RenderPass, Framebuffer, ClearValues);

    {
        const VkExtent3D &Extents = RTLayout.GetExtent3D();
        Context.GetPendingGfxState()->SetViewport(0, 0, 0, Extents.width, Extents.height, 1);
    }
}

void RenderPassManager::EndRenderPass(CmdBuffer *CmdBuffer)
{
    CmdBuffer->EndRenderPass();
    // VulkanRHI::DebugHeavyWeightBarrier(CmdBuffer->GetHandle(), 1);
}

void RenderPassManager::NotifyDeletedRenderTarget(VkImage Image)
{
    std::vector<uint32> toDeleteKey;

    for (auto It = Framebuffers.begin(); It != Framebuffers.end(); ++It)
    {
        FramebufferList *List = It->second;
        for (int32 Index = List->Framebuffer.size() - 1; Index >= 0; --Index)
        {
            Framebuffer *framebuffer = List->Framebuffer[Index];
            if (framebuffer->ContainsRenderTarget(Image))
            {
                List->Framebuffer[Index] = List->Framebuffer.back();
                List->Framebuffer.pop_back();
                framebuffer->Destroy(*device);
                delete framebuffer;
            }
        }

        if (List->Framebuffer.size() == 0)
        {
            delete List;
            toDeleteKey.push_back(It->first);
        }
    }

    for (auto key : toDeleteKey)
    {
        Framebuffers.erase(key);
    }
}