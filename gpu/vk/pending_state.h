#pragma once
#include "vulkan_memory.h"
#include "pipeline.h"
#include "pipeline_state.h"
#include "gpu/RHI/RHIDefinitions.h"
#include <iostream>

class CommandListContext;
class VulkanGraphicsPipelineState;
class GraphicsPipelineDescriptorState;
class Framebuffer;
class VulkanShader;

// All the current gfx pipeline states in use
class PendingGfxState : public VulkanRHI::DeviceChild
{
public:
    PendingGfxState(Device *InDevice, CommandListContext &InContext)
        : VulkanRHI::DeviceChild(InDevice), context(InContext)
    {
        Reset();
    }

    ~PendingGfxState();

    void Reset()
    {
        viewports.resize(1, {});
        scissors.resize(1, {});
        StencilRef = 0;
        bScissorEnable = false;

        CurrentPipeline = nullptr;
        CurrentState = nullptr;
        bDirtyVertexStreams = true;

        primitiveType = PT_Num;

        // #todo-rco: Would this cause issues?
        // FMemory::Memzero(PendingStreams);
    }

    const uint64 GetCurrentShaderKey(ShaderFrequency Frequency) const
    {
        return (CurrentPipeline ? CurrentPipeline->GetShaderKey(Frequency) : 0);
    }

    const uint64 GetCurrentShaderKey(ShaderStage::Stage Stage) const
    {
        return GetCurrentShaderKey(ShaderStage::GetFrequencyForGfxStage(Stage));
    }

    const VulkanShader *GetCurrentShader(ShaderFrequency Frequency) const
    {
        return (CurrentPipeline ? CurrentPipeline->GetShader(Frequency) : nullptr);
    }

    void SetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ)
    {
        viewports.resize(1, {});

        viewports[0].x = MinX;
        viewports[0].y = MinY;
        viewports[0].width = MaxX - MinX;
        viewports[0].height = MaxY - MinY;
        viewports[0].minDepth = MinZ;
        if (MinZ == MaxZ)
        {
            // Engine pases in some cases MaxZ as 0.0
            viewports[0].maxDepth = MinZ + 1.0f;
        }
        else
        {
            viewports[0].maxDepth = MaxZ;
        }

        SetScissorRect((uint32)MinX, (uint32)MinY, (uint32)(MaxX - MinX), (uint32)(MaxY - MinY));
        bScissorEnable = false;
    }

    inline void SetScissorRect(uint32 MinX, uint32 MinY, uint32 Width, uint32 Height)
    {
        scissors.resize(1, {});

        scissors[0].offset.x = MinX;
        scissors[0].offset.y = MinY;
        scissors[0].extent.width = Width;
        scissors[0].extent.height = Height;
    }

    inline void SetStreamSource(uint32 StreamIndex, VkBuffer VertexBuffer, uint32 Offset)
    {
        PendingStreams[StreamIndex].Stream = VertexBuffer;
        PendingStreams[StreamIndex].BufferOffset = Offset;
        bDirtyVertexStreams = true;
    }

    inline void Bind(VkCommandBuffer CmdBuffer) { CurrentPipeline->Bind(CmdBuffer); }

    inline void SetTextureForStage(ShaderStage::Stage Stage, uint32 ParameterIndex, const VulkanTexture *Texture, VkImageLayout Layout)
    {
        const VulkanGfxPipelineDescriptorInfo &DescriptorInfo = CurrentState->GetGfxPipelineDescriptorInfo();
        uint8 DescriptorSet;
        uint32 BindingIndex;
        if (!DescriptorInfo.GetDescriptorSetAndBindingIndex(ShaderHeader::Global, Stage, ParameterIndex, DescriptorSet, BindingIndex))
        {
            return;
        }

        CurrentState->SetTexture(DescriptorSet, BindingIndex, Texture, Layout);
    }

    template <bool bDynamic>
    inline void SetUniformBuffer(uint8 DescriptorSet, uint32 BindingIndex, const VulkanUniformBuffer *UniformBuffer)
    {
        CurrentState->SetUniformBuffer<bDynamic>(DescriptorSet, BindingIndex, UniformBuffer);
    }

    /// @brief 申请descriptor set，并写入。绑定动态状态（视口、裁剪等）。绑定descriptor set
    void PrepareForDraw(CmdBuffer *CmdBuffer);

    bool SetGfxPipeline(VulkanGraphicsPipelineState *InGfxPipeline, bool bForceReset);

    inline void UpdateDynamicStates(CmdBuffer *Cmd) { InternalUpdateDynamicStates(Cmd); }

    inline void SetStencilRef(uint32 InStencilRef)
    {
        if (InStencilRef != StencilRef)
        {
            StencilRef = InStencilRef;
        }
    }

    void NotifyDeletedPipeline(VulkanGraphicsPipelineState *Pipeline)
    {
        PipelineStates.erase(Pipeline);
    }

protected:
    std::vector<VkViewport> viewports;
    std::vector<VkRect2D> scissors;

    PrimitiveType primitiveType = PT_Num;
    uint32 StencilRef;
    bool bScissorEnable;

    VulkanGraphicsPipelineState *CurrentPipeline;
    GraphicsPipelineDescriptorState *CurrentState;

    std::unordered_map<VulkanGraphicsPipelineState *, GraphicsPipelineDescriptorState *> PipelineStates;

    struct VertexStream
    {
        VertexStream() : Stream(VK_NULL_HANDLE), BufferOffset(0) {}

        VkBuffer Stream;
        uint32 BufferOffset;
    };
    VertexStream PendingStreams[MaxVertexElementCount];
    bool bDirtyVertexStreams;

    void InternalUpdateDynamicStates(CmdBuffer *Cmd);
    // void UpdateInputAttachments(Framebuffer *Framebuffer);

    CommandListContext &context;
    friend class CommandListContext;
};