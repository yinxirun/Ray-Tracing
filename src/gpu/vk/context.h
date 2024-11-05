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

    inline bool IsImmediate() const { return Immediate == nullptr; }

    // RHI
    virtual void DrawIndexedPrimitive(Buffer *IndexBuffer, int32 BaseVertexIndex, uint32 FirstInstance,
                                      uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) final override;

    virtual void RHIPushEvent(const char *Name, int Color) final;
    virtual void RHIPopEvent() final;

    virtual void BeginDrawingViewport(std::shared_ptr<Viewport> &Viewport) final override;
    virtual void EndDrawingViewport(Viewport *Viewport, bool bLockToVsync) final override;

    virtual void BeginFrame() final override;
    virtual void EndFrame() final override;

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

    void RequestSubmitCurrentCommands();

public:
    bool IsSwapchainImage(Texture *InTexture) const;
    VkSurfaceTransformFlagBitsKHR GetSwapchainQCOMRenderPassTransform() const;
    SwapChain *GetSwapChain() const;

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
};

class CommandListContextImmediate : public CommandListContext
{
public:
    CommandListContextImmediate(RHI *InRHI, Device *InDevice, Queue *InQueue);
};