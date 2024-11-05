#pragma once

#include "gpu/definitions.h"
#include "RHIResources.h"

class Viewport;
class VulkanTexture;
class Buffer;

class ComputeContext
{
};

class CommandContext
{
public:
    // This method is queued with an RHIThread, otherwise it will flush after it is queued;
    // without an RHI thread there is no benefit to queuing this frame advance commands
    virtual void BeginDrawingViewport(std::shared_ptr<Viewport> &Viewport) = 0;

    // This method is queued with an RHIThread, otherwise it will flush after it is queued;
    // without an RHI thread there is no benefit to queuing this frame advance commands
    virtual void EndDrawingViewport(Viewport *Viewport, bool bLockToVsync) = 0;

    // This method is queued with an RHIThread, otherwise it will flush after it is queued;
    // without an RHI thread there is no benefit to queuing this frame advance commands
    virtual void BeginFrame() = 0;

    // This method is queued with an RHIThread, otherwise it will flush after it is queued;
    // without an RHI thread there is no benefit to queuing this frame advance commands
    virtual void EndFrame() = 0;

    virtual void DrawIndexedPrimitive(Buffer *IndexBuffer, int32 BaseVertexIndex, uint32 FirstInstance,
                                      uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) = 0;

    virtual void BeginRenderPass(const RenderPassInfo &InInfo, const char *InName) = 0;

    virtual void EndRenderPass() = 0;

    virtual void NextSubpass() = 0;

protected:
    RenderPassInfo renderPassInfo;
};