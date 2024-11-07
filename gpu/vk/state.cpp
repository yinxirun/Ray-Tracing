#include "state.h"

static inline VkPolygonMode RasterizerFillModeToVulkan(ERasterizerFillMode InFillMode)
{
    switch (InFillMode)
    {
    case FM_Point:
        return VK_POLYGON_MODE_POINT;
    case FM_Wireframe:
        return VK_POLYGON_MODE_LINE;
    case FM_Solid:
        return VK_POLYGON_MODE_FILL;
    default:
        break;
    }
    check(0);
    return VK_POLYGON_MODE_MAX_ENUM;
}

static inline VkCullModeFlags RasterizerCullModeToVulkan(ERasterizerCullMode InCullMode)
{
    switch (InCullMode)
    {
    case CM_None:
        return VK_CULL_MODE_NONE;
    case CM_CW:
        return VK_CULL_MODE_FRONT_BIT;
    case CM_CCW:
        return VK_CULL_MODE_BACK_BIT;
    default:
        break;
    }
    check(0);
    return VK_CULL_MODE_NONE;
}

static inline VkBlendOp BlendOpToVulkan(EBlendOperation InOp)
{
	switch (InOp)
	{
		case BO_Add:				return VK_BLEND_OP_ADD;
		case BO_Subtract:			return VK_BLEND_OP_SUBTRACT;
		case BO_Min:				return VK_BLEND_OP_MIN;
		case BO_Max:				return VK_BLEND_OP_MAX;
		case BO_ReverseSubtract:	return VK_BLEND_OP_REVERSE_SUBTRACT;
		default:
			break;
	}
	check(0);
	return VK_BLEND_OP_MAX_ENUM;
}

static inline VkBlendFactor BlendFactorToVulkan(EBlendFactor InFactor)
{
	switch (InFactor)
	{
		case BF_Zero:						return VK_BLEND_FACTOR_ZERO;
		case BF_One:						return VK_BLEND_FACTOR_ONE;
		case BF_SourceColor:				return VK_BLEND_FACTOR_SRC_COLOR;
		case BF_InverseSourceColor:			return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case BF_SourceAlpha:				return VK_BLEND_FACTOR_SRC_ALPHA;
		case BF_InverseSourceAlpha:			return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case BF_DestAlpha:					return VK_BLEND_FACTOR_DST_ALPHA;
		case BF_InverseDestAlpha:			return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		case BF_DestColor:					return VK_BLEND_FACTOR_DST_COLOR;
		case BF_InverseDestColor:			return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case BF_ConstantBlendFactor:		return VK_BLEND_FACTOR_CONSTANT_COLOR;
		case BF_InverseConstantBlendFactor:	return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
		case BF_Source1Color:				return VK_BLEND_FACTOR_SRC1_COLOR;
		case BF_InverseSource1Color:		return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
		case BF_Source1Alpha:				return VK_BLEND_FACTOR_SRC1_ALPHA;
		case BF_InverseSource1Alpha:		return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
		default:
			break;
	}

	check(0);
	return VK_BLEND_FACTOR_MAX_ENUM;
}

VulkanRasterizerState::VulkanRasterizerState(const RasterizerStateInitializer &InInitializer)
    : Initializer(InInitializer)
{
    VulkanRasterizerState::ResetCreateInfo(RasterizerState);

    // @todo vulkan: I'm assuming that Solid and Wireframe wouldn't ever be mixed within the same BoundShaderState, so we are ignoring the fill mode as a unique identifier
    // checkf(Initializer.FillMode == FM_Solid, TEXT("PIPELINE KEY: Only FM_Solid is supported fill mode [got %d]"), (int32)Initializer.FillMode);

    RasterizerState.polygonMode = RasterizerFillModeToVulkan(Initializer.FillMode);
    RasterizerState.cullMode = RasterizerCullModeToVulkan(Initializer.CullMode);

    RasterizerState.depthClampEnable = InInitializer.DepthClipMode == ERasterizerDepthClipMode::DepthClamp ? VK_TRUE : VK_FALSE;
    RasterizerState.depthBiasEnable = Initializer.DepthBias != 0.0f ? VK_TRUE : VK_FALSE;

    RasterizerState.depthBiasSlopeFactor = Initializer.SlopeScaleDepthBias;
    RasterizerState.depthBiasConstantFactor = Initializer.DepthBias;
}

VulkanBlendState::VulkanBlendState(const BlendStateInitializerRHI &InInitializer)
{
    Initializer = InInitializer;
    for (uint32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
    {
        const BlendStateInitializerRHI::RenderTarget &ColorTarget = Initializer.RenderTargets[Index];
        VkPipelineColorBlendAttachmentState &BlendState = BlendStates[Index];
        BlendState = {};

        BlendState.colorBlendOp = BlendOpToVulkan(ColorTarget.ColorBlendOp);
        BlendState.alphaBlendOp = BlendOpToVulkan(ColorTarget.AlphaBlendOp);

        BlendState.dstColorBlendFactor = BlendFactorToVulkan(ColorTarget.ColorDestBlend);
        BlendState.dstAlphaBlendFactor = BlendFactorToVulkan(ColorTarget.AlphaDestBlend);

        BlendState.srcColorBlendFactor = BlendFactorToVulkan(ColorTarget.ColorSrcBlend);
        BlendState.srcAlphaBlendFactor = BlendFactorToVulkan(ColorTarget.AlphaSrcBlend);

        BlendState.blendEnable =
            (ColorTarget.ColorBlendOp != BO_Add || ColorTarget.ColorDestBlend != BF_Zero || ColorTarget.ColorSrcBlend != BF_One ||
             ColorTarget.AlphaBlendOp != BO_Add || ColorTarget.AlphaDestBlend != BF_Zero || ColorTarget.AlphaSrcBlend != BF_One)
                ? VK_TRUE
                : VK_FALSE;

        BlendState.colorWriteMask = (ColorTarget.ColorWriteMask & CW_RED) ? VK_COLOR_COMPONENT_R_BIT : 0;
        BlendState.colorWriteMask |= (ColorTarget.ColorWriteMask & CW_GREEN) ? VK_COLOR_COMPONENT_G_BIT : 0;
        BlendState.colorWriteMask |= (ColorTarget.ColorWriteMask & CW_BLUE) ? VK_COLOR_COMPONENT_B_BIT : 0;
        BlendState.colorWriteMask |= (ColorTarget.ColorWriteMask & CW_ALPHA) ? VK_COLOR_COMPONENT_A_BIT : 0;
    }
}
