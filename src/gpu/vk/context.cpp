#include "context.h"
#include "command_buffer.h"
#include "rhi.h"
#include "viewport.h"
#include "private.h"
#include "renderpass.h"
#include "pending_state.h"

extern RHI *globalRHI;

CommandListContext::CommandListContext(RHI *InRHI, Device *InDevice, Queue *InQueue, CommandListContext *InImmediate)
    : rhi(InRHI), Immediate(InImmediate), device(InDevice), queue(InQueue), bSubmitAtNextSafePoint(false),
      commandBufferManager(nullptr), pendingGfxState(nullptr)
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

    // Create Pending state, contains pipeline states such as current shader and etc..
    pendingGfxState = new PendingGfxState(device, *this);
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
    delete pendingGfxState;
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

void CommandListContext::BeginDrawingViewport(std::shared_ptr<Viewport> &Viewport)
{
    check(Viewport);
    globalRHI->drawingViewport = Viewport;
}

void CommandListContext::EndDrawingViewport(Viewport *Viewport, bool bLockToVsync)
{
    check(IsImmediate());
    CmdBuffer *cmdBuffer = commandBufferManager->GetActiveCmdBuffer();
    check(!cmdBuffer->HasEnded() && !cmdBuffer->IsInsideRenderPass());

    Viewport->Present(this, cmdBuffer, queue, device->GetPresentQueue(), bLockToVsync);
    globalRHI->drawingViewport = nullptr;
}

void CommandListContext::BeginFrame()
{
    check(IsImmediate());

    extern uint32 GVulkanRHIDeletionFrameNumber;
    ++GVulkanRHIDeletionFrameNumber;

#if VULKAN_RHI_RAYTRACING
    if (GRHISupportsRayTracing)
    {
        Device->GetRayTracingCompactionRequestHandler()->Update(*this);
    }
#endif
}

void CommandListContext::EndFrame()
{
    check(IsImmediate());

    bool bTrimMemory = false;
    GetCommandBufferManager()->FreeUnusedCmdBuffers(bTrimMemory);

    // device->GetStagingManager().ProcessPendingFree(false, true);
    // device->GetMemoryManager().ReleaseFreedPages(*this);
    device->GetDeferredDeletionQueue().ReleaseResources();

    if (UseVulkanDescriptorCache())
    {
        printf("ERROR: Don't support Descriptor Set Cache %s %d\n", __FILE__, __LINE__);
        exit(-1);
        // device->GetDescriptorSetCache().GC();
    }
    // device->GetDescriptorPoolsManager().GC();

    // device->ReleaseUnusedOcclusionQueryPools();

    // device->GetPipelineStateCache()->TickLRU();
}

CommandListContextImmediate::CommandListContextImmediate(RHI *InRHI, Device *InDevice, Queue *InQueue)
    : CommandListContext(InRHI, InDevice, InQueue, nullptr) {}