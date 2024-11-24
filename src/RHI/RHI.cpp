#include "RHI.h"
#include "core/templates/type_hash.h"
uint32 GetTypeHash(const BlendStateInitializerRHI::RenderTarget &Initializer)
{
    uint32 Hash = GetTypeHash(Initializer.ColorBlendOp);
    Hash = HashCombine(Hash, GetTypeHash(Initializer.ColorDestBlend));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.ColorSrcBlend));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.AlphaBlendOp));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.AlphaDestBlend));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.AlphaSrcBlend));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.ColorWriteMask));
    return Hash;
}

uint32 GetTypeHash(const BlendStateInitializerRHI &Initializer)
{
    uint32 Hash = GetTypeHash(Initializer.bUseIndependentRenderTargetBlendStates);
    Hash = HashCombine(Hash, Initializer.bUseAlphaToCoverage);
    for (int32 i = 0; i < MaxSimultaneousRenderTargets; ++i)
    {
        Hash = HashCombine(Hash, GetTypeHash(Initializer.RenderTargets[i]));
    }

    return Hash;
}

uint32 GetTypeHash(const RasterizerStateInitializer &Initializer)
{
    uint32 Hash = GetTypeHash(Initializer.FillMode);
    Hash = HashCombine(Hash, GetTypeHash(Initializer.CullMode));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.DepthBias));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.SlopeScaleDepthBias));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.DepthClipMode));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.bAllowMSAA));
    return Hash;
}

uint32 GetTypeHash(const DepthStencilStateInitializer &Initializer)
{
    uint32 Hash = GetTypeHash(Initializer.bEnableDepthWrite);
    Hash = HashCombine(Hash, GetTypeHash(Initializer.DepthTest));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.bEnableFrontFaceStencil));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.FrontFaceStencilTest));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.FrontFaceStencilFailStencilOp));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.FrontFaceDepthFailStencilOp));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.FrontFacePassStencilOp));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.bEnableBackFaceStencil));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.BackFaceStencilTest));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.BackFaceStencilFailStencilOp));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.BackFaceDepthFailStencilOp));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.BackFacePassStencilOp));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.StencilReadMask));
    Hash = HashCombine(Hash, GetTypeHash(Initializer.StencilWriteMask));
    return Hash;
}