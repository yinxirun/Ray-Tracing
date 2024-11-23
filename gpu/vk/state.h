#pragma once

#include "gpu/RHI/RHIResources.h"
#include "gpu/RHI/RHI.h"
#include "common.h"

class Device;

class VulkanSamplerState : public SamplerState
{
public:
    VulkanSamplerState(const VkSamplerCreateInfo &InInfo, Device &InDevice, const bool bInIsImmutable = false);

    virtual bool IsImmutable() const final override { return bIsImmutable; }

    VkSampler sampler;
    uint32 SamplerId;
    RHIDescriptorHandle BindlessHandle;

    static void SetupSamplerCreateInfo(const SamplerStateInitializer &Initializer, Device &InDevice, VkSamplerCreateInfo &OutSamplerInfo);

    virtual RHIDescriptorHandle GetBindlessHandle() const override final { return BindlessHandle; }

private:
    bool bIsImmutable;
};

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
    VulkanDepthStencilState(const DepthStencilStateInitializer &InInitializer)
    {
        Initializer = InInitializer;
    }
    void SetupCreateInfo(const GraphicsPipelineStateInitializer &GfxPSOInit,
                         VkPipelineDepthStencilStateCreateInfo &OutDepthStencilState);
    DepthStencilStateInitializer Initializer;
};

class VulkanBlendState : public BlendState
{
public:
    VulkanBlendState(const BlendStateInitializerRHI &InInitializer);

    // array the pipeline state can point right to
    VkPipelineColorBlendAttachmentState BlendStates[MaxSimultaneousRenderTargets];

    BlendStateInitializerRHI Initializer;
};
