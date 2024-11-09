#include "RHIResources.h"
#include "RHIDefinitions.h"
#include <atomic>

RHIResource::RHIResource(ERHIResourceType InResourceType) : ResourceType(InResourceType) {}

RHIResource::~RHIResource()
{
    assert(numRefs.load() == 0);
}

RHIViewableResource::RHIViewableResource(ERHIResourceType InResourceType, Access InAccess)
    : RHIResource(InResourceType), TrackedAccess(InAccess)
{
}

Texture::Texture(const TextureCreateDesc &InDesc) : RHIViewableResource(RRT_Texture, InDesc.InitialState), textureDesc(InDesc)
{
}

void RenderPassInfo::ConvertToRenderTargetsInfo(SetRenderTargetsInfo &OutRTInfo) const
{
    for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
    {
        if (!ColorRenderTargets[Index].RenderTarget)
        {
            break;
        }

        OutRTInfo.ColorRenderTarget[Index].texture = ColorRenderTargets[Index].RenderTarget;
        RenderTargetLoadAction LoadAction = GetLoadAction(ColorRenderTargets[Index].Action);
        OutRTInfo.ColorRenderTarget[Index].LoadAction = LoadAction;
        OutRTInfo.ColorRenderTarget[Index].StoreAction = GetStoreAction(ColorRenderTargets[Index].Action);
        OutRTInfo.ColorRenderTarget[Index].ArraySliceIndex = ColorRenderTargets[Index].ArraySlice;
        OutRTInfo.ColorRenderTarget[Index].MipIndex = ColorRenderTargets[Index].MipIndex;
        ++OutRTInfo.NumColorRenderTargets;

        OutRTInfo.bClearColor |= (LoadAction == RenderTargetLoadAction::EClear);

        if (ColorRenderTargets[Index].ResolveTarget)
        {
            OutRTInfo.bHasResolveAttachments = true;
            OutRTInfo.ColorResolveRenderTarget[Index] = OutRTInfo.ColorRenderTarget[Index];
            OutRTInfo.ColorResolveRenderTarget[Index].texture = ColorRenderTargets[Index].ResolveTarget;
        }
    }

    RenderTargetActions DepthActions = GetDepthActions(DepthStencilRenderTarget.Action);
    RenderTargetActions StencilActions = GetStencilActions(DepthStencilRenderTarget.Action);
    RenderTargetLoadAction DepthLoadAction = GetLoadAction(DepthActions);
    RenderTargetStoreAction DepthStoreAction = GetStoreAction(DepthActions);
    RenderTargetLoadAction StencilLoadAction = GetLoadAction(StencilActions);
    RenderTargetStoreAction StencilStoreAction = GetStoreAction(StencilActions);

    OutRTInfo.DepthStencilRenderTarget = RHIDepthRenderTargetView(DepthStencilRenderTarget.DepthStencilTarget,
                                                                  DepthLoadAction,
                                                                  GetStoreAction(DepthActions),
                                                                  StencilLoadAction,
                                                                  GetStoreAction(StencilActions),
                                                                  DepthStencilRenderTarget.ExclusiveDepthStencil);
    OutRTInfo.bClearDepth = (DepthLoadAction == RenderTargetLoadAction::EClear);
    OutRTInfo.bClearStencil = (StencilLoadAction == RenderTargetLoadAction::EClear);

    // OutRTInfo.ShadingRateTexture = ShadingRateTexture;
    // OutRTInfo.ShadingRateTextureCombiner = ShadingRateTextureCombiner;
    OutRTInfo.MultiViewCount = MultiViewCount;
}