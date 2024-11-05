#include "context.h"

void CommandListContext::WriteGPUFence(GPUFence *FenceRHI)
{
    CmdBuffer *CmdBuffer = commandBufferManager->GetActiveCmdBuffer();
    VulkanGPUFence *Fence = dynamic_cast<VulkanGPUFence*>(FenceRHI);

    Fence->CmdBuffer = CmdBuffer;
    Fence->FenceSignaledCounter = CmdBuffer->GetFenceSignaledCounter();
}