#include "context.h"
#include "command_buffer.h"
#include "rhi.h"
#include "viewport.h"

extern RHI *globalRHI;

CommandListContext::CommandListContext(RHI *InRHI, Device *InDevice, Queue *InQueue, CommandListContext *InImmediate)
    : rhi(InRHI), device(InDevice), queue(InQueue), Immediate(InImmediate)
{
    // Create CommandBufferManager, contain all active buffers
    commandBufferManager = new CommandBufferManager(InDevice, this);
    commandBufferManager->Init(this);

    if (IsImmediate())
    {
        // Flush the cmd buffer immediately to ensure a valid
        // 'Last submitted' cmd buffer exists at frame 0.
        commandBufferManager->SubmitActiveCmdBuffer();
        commandBufferManager->PrepareForNewActiveCommandBuffer();
    }
}

CommandListContext::~CommandListContext()
{
    // if (FVulkanPlatform::SupportsTimestampRenderQueries())
    // {
    //     FrameTiming->Release();
    //     delete FrameTiming;
    //     FrameTiming = nullptr;
    // }

    check(commandBufferManager != nullptr);
    delete commandBufferManager;
    commandBufferManager = nullptr;

    // delete UniformBufferUploader;
    // delete PendingGfxState;
    // delete PendingComputeState;

    // TempFrameAllocationBuffer.Destroy();
}

void CommandListContext::RHIPushEvent(const char *Name, int Color)
{
#ifdef PRINT_UNIMPLEMENT
    printf("Have not implement CommandListContext::RHIPushEvent %s %d\n", __FILE__, __LINE__);
#endif
}

void CommandListContext::RHIPopEvent()
{
#ifdef PRINT_UNIMPLEMENT
    printf("Have not implement CommandListContext::RHIPopEvent %s %d\n", __FILE__, __LINE__);
#endif
}

void CommandListContext::RHIBeginDrawingViewport(std::shared_ptr<Viewport> &Viewport)
{
    check(Viewport);
    globalRHI->drawingViewport = Viewport;
}

void CommandListContext::RHIEndDrawingViewport(Viewport *Viewport, bool bLockToVsync)
{
    check(IsImmediate());
    CmdBuffer *cmdBuffer = commandBufferManager->GetActiveCmdBuffer();
    check(!cmdBuffer->HasEnded() && !cmdBuffer->IsInsideRenderPass());

    Viewport->Present(this, cmdBuffer, queue, device->GetPresentQueue(), bLockToVsync);
    globalRHI->drawingViewport = nullptr;
}

CommandListContextImmediate::CommandListContextImmediate(RHI *InRHI, Device *InDevice, Queue *InQueue)
    : CommandListContext(InRHI, InDevice, InQueue, nullptr) {}