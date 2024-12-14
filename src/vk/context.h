/// @file 一个 CommandListContext 对应着一个 CommandBufferManager，也对应着一个 CommandBufferPool 。这三者一一对应。
#pragma once
#include <memory>
#include <iostream>
#include "RHI/RHIContext.h"
#include "private.h"
#include "queue.h"

class RHI;
class Device;
class Queue;
class CommandBufferManager;
class VulkanViewport;
class RenderPass;
class Framebuffer;
class PendingGfxState;
class PendingComputeState;
class SwapChain;
class VulkanUniformBufferUploader;

class CommandListContext : public CommandContext
{
public:
    CommandListContext(RHI *InRHI, Device *InDevice, Queue *InQueue, CommandListContext *InImmediate);
    virtual ~CommandListContext();

    static inline CommandListContext &GetVulkanContext(CommandContext &CmdContext)
    {
        return static_cast<CommandListContext &>(CmdContext.GetLowestLevelContext());
    }

    inline bool IsImmediate() const { return Immediate == nullptr; }

    template <class ShaderType>
    void SetResourcesFromTables(const ShaderType *RESTRICT);
    void CommitGraphicsResourceTables();
    void CommitComputeResourceTables();

    // RHI
    virtual void SetShaderSampler(GraphicsShader *Shader, uint32 SamplerIndex, SamplerState *NewState) final override;
    virtual void SetShaderTexture(GraphicsShader *Shader, uint32 TextureIndex, Texture *NewTexture) final override;
    virtual void SetShaderUniformBuffer(GraphicsShader *Shader, uint32 BufferIndex, UniformBuffer *Buffer) final override;
    virtual void DrawIndexedPrimitive(Buffer *IndexBuffer, int32 BaseVertexIndex, uint32 FirstInstance,
                                      uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) final override;

    virtual void RHIPushEvent(const char *Name, int Color) final;
    virtual void RHIPopEvent() final;

    virtual void SetComputePipelineState(ComputePipelineState* ComputePipelineState) final override;
	virtual void DispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override;

    virtual void WriteGPUFence(GPUFence *Fence) final override;

    virtual void SubmitCommandsHint() final override;

    void BeginDrawingViewport(std::shared_ptr<VulkanViewport> &Viewport) final override;
    void EndDrawingViewport(VulkanViewport *Viewport, bool bLockToVsync) final override;

    void BeginFrame() final override;
    void EndFrame() final override;

    void SetStreamSource(uint32 StreamIndex, Buffer *VertexBuffer, uint32 Offset) final override;
    void SetGraphicsPipelineState(GraphicsPipelineState *GraphicsState, uint32 StencilRef, bool bApplyAdditionalState) final override;

    virtual void BeginRenderPass(const RenderPassInfo &InInfo, const char *InName) final override;
    virtual void EndRenderPass() final override;
    virtual void NextSubpass() final override;

    inline CommandBufferManager *GetCommandBufferManager() { return commandBufferManager; }

    inline VulkanRHI::TempFrameAllocationBuffer &GetTempFrameAllocationBuffer() { return tempFrameAllocationBuffer; }

    inline PendingGfxState *GetPendingGfxState() { return pendingGfxState; }

    inline void NotifyDeletedRenderTarget(VkImage Image)
    {
        if (CurrentFramebuffer && CurrentFramebuffer->ContainsRenderTarget(Image))
        {
            CurrentFramebuffer = nullptr;
        }
    }

    inline void NotifyDeletedImage(VkImage Image)
    {
        commandBufferManager->NotifyDeletedImage(Image);
        queue->NotifyDeletedImage(Image);
    }

    inline RenderPass *GetCurrentRenderPass() { return CurrentRenderPass; }

    inline Framebuffer *GetCurrentFramebuffer() { return CurrentFramebuffer; }

    inline VulkanUniformBufferUploader *GetUniformBufferUploader() { return UniformBufferUploader; }

    inline Queue *GetQueue() { return queue; }

    inline Device *GetDevice() const { return device; }

    void ReleasePendingState();

protected:
    RHI *rhi;
    CommandListContext *Immediate;
    Device *device;
    CommandBufferManager *commandBufferManager;
    Queue *queue;
    bool bSubmitAtNextSafePoint;
    VulkanUniformBufferUploader *UniformBufferUploader;

    VulkanRHI::TempFrameAllocationBuffer tempFrameAllocationBuffer;

    RenderPass *CurrentRenderPass = nullptr;
    Framebuffer *CurrentFramebuffer = nullptr;

    PendingGfxState *pendingGfxState;
	PendingComputeState* pendingComputeState;

    // Match the D3D12 maximum of 16 constant buffers per shader stage.
    enum
    {
        MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE = 16
    };

    // Track the currently bound uniform buffers.
    VulkanUniformBuffer *BoundUniformBuffers[SF_NumStandardFrequencies][MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE] = {};

    // Bit array to track which uniform buffers have changed since the last draw call.
    uint16 DirtyUniformBuffers[SF_NumStandardFrequencies] = {};

    void PrepareForCPURead();
    void RequestSubmitCurrentCommands();

public:
    bool IsSwapchainImage(Texture *InTexture) const;
    VkSurfaceTransformFlagBitsKHR GetSwapchainQCOMRenderPassTransform() const;
    SwapChain *GetSwapChain() const;

    RenderPass *PrepareRenderPassForPSOCreation(const GraphicsPipelineStateInitializer &Initializer);
    RenderPass *PrepareRenderPassForPSOCreation(const RenderTargetLayout &Initializer);

private:
    inline bool SafePointSubmit()
    {
#ifdef PRINT_TO_EXPLORE
        printf("What does it mean? %s %d\n", __FILE__, __LINE__);
#endif
        if (bSubmitAtNextSafePoint)
        {
            InternalSubmitActiveCmdBuffer();
            bSubmitAtNextSafePoint = false;
            return true;
        }
        return false;
    }
    void InternalSubmitActiveCmdBuffer();

    friend class Device;
};

class CommandListContextImmediate : public CommandListContext
{
public:
    CommandListContextImmediate(RHI *InRHI, Device *InDevice, Queue *InQueue);
};