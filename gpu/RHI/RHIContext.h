#pragma once

#include "gpu/definitions.h"
#include "gpu/core/assertion_macros.h"
#include "gpu/RHI/multi_gpu.h"
#include "RHIResources.h"

class VulkanViewport;
class VulkanTexture;
class Buffer;
class GraphicsPipelineState;

class ComputeContext
{
public:
    /// Always returns the platform RHI context, even when the validation RHI is active.
    virtual ComputeContext &GetLowestLevelContext() { return *this; }

    /// Write the fence in the GPU timeline. The fence can then be tested on the CPU to know if the previous GPU commands are completed.
    virtual void WriteGPUFence(GPUFence *FenceRHI)
    {
        check(false);
    }

    /** Set the shader resource view of a surface. */
    virtual void SetShaderTexture(GraphicsShader *Shader, uint32 TextureIndex, Texture *NewTexture) = 0;

    /// Sets sampler state.
    /// @param Shader The shader to set the sampler for.
    /// @param SamplerIndex The index of the sampler.
    /// @param NewState The new sampler state.
    virtual void SetShaderSampler(GraphicsShader *Shader, uint32 SamplerIndex, SamplerState *NewState) = 0;

    /// Submit the current command buffer to the GPU if possible.
    virtual void SubmitCommandsHint() = 0;

    virtual void SetGPUMask(RHIGPUMask GPUMask) { ensure(GPUMask == RHIGPUMask::GPU0()); }
    virtual RHIGPUMask GetGPUMask() const { return RHIGPUMask::GPU0(); }
};

class CommandContext : public ComputeContext
{
public:
    // This method is queued with an RHIThread, otherwise it will flush after it is queued;
    // without an RHI thread there is no benefit to queuing this frame advance commands
    virtual void BeginDrawingViewport(std::shared_ptr<VulkanViewport> &Viewport) = 0;

    // This method is queued with an RHIThread, otherwise it will flush after it is queued;
    // without an RHI thread there is no benefit to queuing this frame advance commands
    virtual void EndDrawingViewport(VulkanViewport *Viewport, bool bLockToVsync) = 0;

    // This method is queued with an RHIThread, otherwise it will flush after it is queued;
    // without an RHI thread there is no benefit to queuing this frame advance commands
    virtual void BeginFrame() = 0;

    // This method is queued with an RHIThread, otherwise it will flush after it is queued;
    // without an RHI thread there is no benefit to queuing this frame advance commands
    virtual void EndFrame() = 0;

    virtual void SetStreamSource(uint32 StreamIndex, Buffer *VertexBuffer, uint32 Offset) = 0;

    virtual void SetGraphicsPipelineState(GraphicsPipelineState *GraphicsState, uint32 StencilRef, bool bApplyAdditionalState) = 0;

    virtual void SetShaderUniformBuffer(GraphicsShader *Shader, uint32 BufferIndex, UniformBuffer *Buffer) = 0;

    virtual void DrawIndexedPrimitive(Buffer *IndexBuffer, int32 BaseVertexIndex, uint32 FirstInstance,
                                      uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) = 0;

    virtual void BeginRenderPass(const RenderPassInfo &InInfo, const char *InName) = 0;

    virtual void EndRenderPass() = 0;

    virtual void NextSubpass() = 0;

protected:
    RenderPassInfo renderPassInfo;
};