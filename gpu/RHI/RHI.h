#pragma once

#include "RHIDefinitions.h"
#include "gpu/core/containers/enum_as_byte.h"
#include <array>
#include <vector>

class RHICommandListImmediate;

class DynamicRHI
{
public:
    /** Declare a virtual destructor, so the dynamic RHI can be deleted without knowing its type. */
    virtual ~DynamicRHI() {}

    /** Initializes the RHI; separate from IDynamicRHIModule::CreateRHI so that GDynamicRHI is set when it is called. */
    virtual void Init() = 0;

    /** Shutdown the RHI; handle shutdown and resource destruction before the RHI's actual destructor is called (so that all resources of the RHI are still available for shutdown). */
    virtual void Shutdown() = 0;

    /////// RHI Methods

    virtual void BeginFrame(RHICommandListImmediate &RHICmdList) = 0;
};

// 164
struct VertexElement
{
    uint8 StreamIndex;
    uint8 Offset;
    TEnumAsByte<VertexElementType> Type;
    uint8 AttributeIndex;
    uint16 Stride;
    /**
     * Whether to use instance index or vertex index to consume the element.
     * eg if bUseInstanceIndex is 0, the element will be repeated for every instance.
     */
    uint16 bUseInstanceIndex;

    VertexElement() {}
    VertexElement(uint8 InStreamIndex, uint8 InOffset, VertexElementType InType, uint8 InAttributeIndex,
                  uint16 InStride, bool bInUseInstanceIndex = false) : StreamIndex(InStreamIndex),
                                                                       Offset(InOffset),
                                                                       Type(InType),
                                                                       AttributeIndex(InAttributeIndex),
                                                                       Stride(InStride),
                                                                       bUseInstanceIndex(bInUseInstanceIndex) {}

    bool operator==(const VertexElement &Other) const
    {
        return (StreamIndex == Other.StreamIndex &&
                Offset == Other.Offset &&
                Type == Other.Type &&
                AttributeIndex == Other.AttributeIndex &&
                Stride == Other.Stride &&
                bUseInstanceIndex == Other.bUseInstanceIndex);
    }
};

struct RasterizerStateInitializer
{
    TEnumAsByte<ERasterizerFillMode> FillMode = FM_Point;
    TEnumAsByte<ERasterizerCullMode> CullMode = CM_None;
    float DepthBias = 0.0f;
    float SlopeScaleDepthBias = 0.0f;
    ERasterizerDepthClipMode DepthClipMode = ERasterizerDepthClipMode::DepthClip;
    bool bAllowMSAA = false;

    RasterizerStateInitializer() = default;
    RasterizerStateInitializer(ERasterizerFillMode InFillMode,
                               ERasterizerCullMode InCullMode,
                               float InDepthBias, float InSlopeScaleDepthBias,
                               ERasterizerDepthClipMode InDepthClipMode, bool bInAllowMSAA)
        : FillMode(InFillMode), CullMode(InCullMode),
          DepthBias(InDepthBias), SlopeScaleDepthBias(InSlopeScaleDepthBias),
          DepthClipMode(InDepthClipMode), bAllowMSAA(bInAllowMSAA) {}

    friend uint32 GetTypeHash(const RasterizerStateInitializer &Initializer);
};

struct DepthStencilStateInitializerRHI
{
    bool bEnableDepthWrite;
    TEnumAsByte<ECompareFunction> DepthTest;

    bool bEnableFrontFaceStencil;
    TEnumAsByte<ECompareFunction> FrontFaceStencilTest;
    TEnumAsByte<EStencilOp> FrontFaceStencilFailStencilOp;
    TEnumAsByte<EStencilOp> FrontFaceDepthFailStencilOp;
    TEnumAsByte<EStencilOp> FrontFacePassStencilOp;
    bool bEnableBackFaceStencil;
    TEnumAsByte<ECompareFunction> BackFaceStencilTest;
    TEnumAsByte<EStencilOp> BackFaceStencilFailStencilOp;
    TEnumAsByte<EStencilOp> BackFaceDepthFailStencilOp;
    TEnumAsByte<EStencilOp> BackFacePassStencilOp;
    uint8 StencilReadMask;
    uint8 StencilWriteMask;

    DepthStencilStateInitializerRHI(
        bool bInEnableDepthWrite = true,
        ECompareFunction InDepthTest = CF_LessEqual,
        bool bInEnableFrontFaceStencil = false,
        ECompareFunction InFrontFaceStencilTest = CF_Always,
        EStencilOp InFrontFaceStencilFailStencilOp = SO_Keep,
        EStencilOp InFrontFaceDepthFailStencilOp = SO_Keep,
        EStencilOp InFrontFacePassStencilOp = SO_Keep,
        bool bInEnableBackFaceStencil = false,
        ECompareFunction InBackFaceStencilTest = CF_Always,
        EStencilOp InBackFaceStencilFailStencilOp = SO_Keep,
        EStencilOp InBackFaceDepthFailStencilOp = SO_Keep,
        EStencilOp InBackFacePassStencilOp = SO_Keep,
        uint8 InStencilReadMask = 0xFF,
        uint8 InStencilWriteMask = 0xFF)
        : bEnableDepthWrite(bInEnableDepthWrite), DepthTest(InDepthTest), bEnableFrontFaceStencil(bInEnableFrontFaceStencil),
          FrontFaceStencilTest(InFrontFaceStencilTest), FrontFaceStencilFailStencilOp(InFrontFaceStencilFailStencilOp),
          FrontFaceDepthFailStencilOp(InFrontFaceDepthFailStencilOp), FrontFacePassStencilOp(InFrontFacePassStencilOp),
          bEnableBackFaceStencil(bInEnableBackFaceStencil), BackFaceStencilTest(InBackFaceStencilTest),
          BackFaceStencilFailStencilOp(InBackFaceStencilFailStencilOp), BackFaceDepthFailStencilOp(InBackFaceDepthFailStencilOp),
          BackFacePassStencilOp(InBackFacePassStencilOp), StencilReadMask(InStencilReadMask),
          StencilWriteMask(InStencilWriteMask)
    {
    }

    friend uint32 GetTypeHash(const DepthStencilStateInitializerRHI& Initializer);
};

class BlendStateInitializerRHI
{
public:
    struct RenderTarget
    {
        // enum
        // {
        // 	NUM_STRING_FIELDS = 7
        // };
        TEnumAsByte<EBlendOperation> ColorBlendOp;
        TEnumAsByte<EBlendFactor> ColorSrcBlend;
        TEnumAsByte<EBlendFactor> ColorDestBlend;
        TEnumAsByte<EBlendOperation> AlphaBlendOp;
        TEnumAsByte<EBlendFactor> AlphaSrcBlend;
        TEnumAsByte<EBlendFactor> AlphaDestBlend;
        TEnumAsByte<EColorWriteMask> ColorWriteMask;

        RenderTarget(EBlendOperation InColorBlendOp = BO_Add, EBlendFactor InColorSrcBlend = BF_One,
                     EBlendFactor InColorDestBlend = BF_Zero, EBlendOperation InAlphaBlendOp = BO_Add,
                     EBlendFactor InAlphaSrcBlend = BF_One, EBlendFactor InAlphaDestBlend = BF_Zero,
                     EColorWriteMask InColorWriteMask = CW_RGBA)
            : ColorBlendOp(InColorBlendOp), ColorSrcBlend(InColorSrcBlend),
              ColorDestBlend(InColorDestBlend), AlphaBlendOp(InAlphaBlendOp),
              AlphaSrcBlend(InAlphaSrcBlend), AlphaDestBlend(InAlphaDestBlend), ColorWriteMask(InColorWriteMask) {}
    };

    BlendStateInitializerRHI() {}

    template <uint64 NumRenderTargets>
    BlendStateInitializerRHI(const std::array<RenderTarget, NumRenderTargets> &InRenderTargetBlendStates, bool bInUseAlphaToCoverage = false)
        : bUseIndependentRenderTargetBlendStates(NumRenderTargets > 1), bUseAlphaToCoverage(bInUseAlphaToCoverage)
    {
        static_assert(NumRenderTargets <= MaxSimultaneousRenderTargets, "Too many render target blend states.");

        for (uint32 RenderTargetIndex = 0; RenderTargetIndex < NumRenderTargets; ++RenderTargetIndex)
        {
            RenderTargets[RenderTargetIndex] = InRenderTargetBlendStates[RenderTargetIndex];
        }
    }

    std::array<RenderTarget, MaxSimultaneousRenderTargets> RenderTargets;
    bool bUseIndependentRenderTargetBlendStates;
    bool bUseAlphaToCoverage;

    friend uint32 GetTypeHash(const BlendStateInitializerRHI &Initializer);
};
