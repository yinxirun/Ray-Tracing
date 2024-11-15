#include "queue.h"
#include "util.h"
/**
 * Vulkan.WaitForIdleOnSubmit
 * Waits for the GPU to be idle after submitting a command buffer. Useful for tracking GPU hangs.
 * 0: Do not wait(default)
 * 1: Wait on every submit
 * 2: Wait when submitting an upload buffer
 * 3: Wait when submitting an active buffer (one that has gfx commands)
 */
int32 GWaitForIdleOnSubmit = 0;

void Queue::Submit(CmdBuffer *CmdBuffer, uint32 NumSignalSemaphores, VkSemaphore *SignalSemaphores)
{
	check(CmdBuffer->HasEnded());

	VulkanRHI::Fence *Fence = CmdBuffer->fence;
	check(!Fence->IsSignaled());

	const VkCommandBuffer CmdBuffers[] = {CmdBuffer->GetHandle()};

	VkSubmitInfo SubmitInfo;
	ZeroVulkanStruct(SubmitInfo, VK_STRUCTURE_TYPE_SUBMIT_INFO);
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = CmdBuffers;
	SubmitInfo.signalSemaphoreCount = NumSignalSemaphores;
	SubmitInfo.pSignalSemaphores = SignalSemaphores;

	std::vector<VkSemaphore> WaitSemaphores;
	if (CmdBuffer->WaitSemaphores.size() > 0)
	{
		for (VulkanRHI::Semaphore *Semaphore : CmdBuffer->WaitSemaphores)
		{
			WaitSemaphores.push_back(Semaphore->GetHandle());
		}
		SubmitInfo.waitSemaphoreCount = (uint32)CmdBuffer->WaitSemaphores.size();
		SubmitInfo.pWaitSemaphores = WaitSemaphores.data();
		SubmitInfo.pWaitDstStageMask = CmdBuffer->WaitFlags.data();
	}
	{
		// SCOPE_CYCLE_COUNTER(STAT_VulkanQueueSubmit);
		VERIFYVULKANRESULT(vkQueueSubmit(queue, 1, &SubmitInfo, Fence->GetHandle()));
	}

	CmdBuffer->State = CmdBuffer::EState::Submitted;
	CmdBuffer->MarkSemaphoresAsSubmitted();
	CmdBuffer->SubmittedFenceCounter = CmdBuffer->FenceSignaledCounter;

	UpdateLastSubmittedCommandBuffer(CmdBuffer);

	CmdBuffer->GetOwner()->RefreshFenceStatus(CmdBuffer);

	device->GetStagingManager().ProcessPendingFree(false, false);

	// If we're tracking layouts for the queue, merge in the changes recorded in this command buffer's context
	CmdBuffer->GetLayoutManager().TransferTo(LayoutManager);
}

void Queue::NotifyDeletedImage(VkImage Image)
{
	LayoutManager.NotifyDeletedImage(Image);
}

void Queue::UpdateLastSubmittedCommandBuffer(CmdBuffer *CmdBuffer)
{
	LastSubmittedCmdBuffer = CmdBuffer;
	LastSubmittedCmdBufferFenceCounter = CmdBuffer->GetFenceSignaledCounter();
}