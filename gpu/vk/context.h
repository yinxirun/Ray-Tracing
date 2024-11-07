/// @file 一个 CommandListContext 对应着一个 CommandBufferManager，也对应着一个 CommandBufferPool 。这三者一一对应。\
///
#pragma once
#include <memory>
#include <iostream>
#include <gpu/RHI/RHIContext.h>
#include "private.h"
#include "queue.h"

class RHI;
class Device;
class Queue;
class CommandBufferManager;
class Viewport;
class RenderPass;
class Framebuffer;
class PendingGfxState;
class SwapChain;

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

    // RHI
    virtual void DrawIndexedPrimitive(Buffer *IndexBuffer, int32 BaseVertexIndex, uint32 FirstInstance,
                                      uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) final override;

    virtual void RHIPushEvent(const char *Name, int Color) final;
    virtual void RHIPopEvent() final;

    virtual void WriteGPUFence(GPUFence *Fence) final override;

    void BeginDrawingViewport(std::shared_ptr<Viewport> &Viewport) final override;
    void EndDrawingViewport(Viewport *Viewport, bool bLockToVsync) final override;

    void BeginFrame() final override;
    void EndFrame() final override;

    void SetStreamSource(uint32 StreamIndex, Buffer *VertexBuffer, uint32 Offset) final override;

    virtual void BeginRenderPass(const RenderPassInfo &InInfo, const char *InName) final override;
    virtual void EndRenderPass() final override;
    virtual void NextSubpass() final override;

    inline CommandBufferManager *GetCommandBufferManager() { return commandBufferManager; }

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

    RenderPass *CurrentRenderPass = nullptr;
    Framebuffer *CurrentFramebuffer = nullptr;

    PendingGfxState *pendingGfxState;

    void PrepareForCPURead();
    void RequestSubmitCurrentCommands();

public:
    bool IsSwapchainImage(Texture *InTexture) const;
    VkSurfaceTransformFlagBitsKHR GetSwapchainQCOMRenderPassTransform() const;
    SwapChain *GetSwapChain() const;

    RenderPass* PrepareRenderPassForPSOCreation(const GraphicsPipelineStateInitializer& Initializer);
	RenderPass* PrepareRenderPassForPSOCreation(const RenderTargetLayout& Initializer);

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