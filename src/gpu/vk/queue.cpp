#include "queue.h"
#include "util.h"

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

	//device->GetStagingManager().ProcessPendingFree(false, false);

	// If we're tracking layouts for the queue, merge in the changes recorded in this command buffer's context
	CmdBuffer->GetLayoutManager().TransferTo(LayoutManager);
}

void Queue::UpdateLastSubmittedCommandBuffer(CmdBuffer *CmdBuffer) {}