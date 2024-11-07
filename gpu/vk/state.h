#pragma once

#include "gpu/RHI/RHIResources.h"
#include "gpu/RHI/RHI.h"
#include "common.h"

class VulkanRasterizerState : public RasterizerState
{
public:
    VulkanRasterizerState(const RasterizerStateInitializer &InInitializer);
    static void ResetCreateInfo(VkPipelineRasterizationStateCreateInfo &OutInfo)
    {
        ZeroVulkanStruct(OutInfo, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
        OutInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
        OutInfo.lineWidth = 1.0f;
    }

    VkPipelineRasterizationStateCreateInfo RasterizerState;
    RasterizerStateInitializer Initializer;
};

class VulkanDepthStencilState : public DepthStencilState
{
public:
	VulkanDepthStencilState(const DepthStencilStateInitializerRHI& InInitializer)
	{
		Initializer = InInitializer;
	}
	DepthStencilStateInitializerRHI Initializer;
};


class VulkanBlendState : public BlendState
{
public:
	VulkanBlendState(const BlendStateInitializerRHI& InInitializer);

	// array the pipeline state can point right to
	VkPipelineColorBlendAttachmentState BlendStates[MaxSimultaneousRenderTargets];

	BlendStateInitializerRHI Initializer;
};
