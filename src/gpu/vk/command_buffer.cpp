#include "command_buffer.h"
#include "vulkan_memory.h"
#include "../definitions.h"
#include "configuration.h"
#include "descriptor_sets.h"
#include "util.h"
#include "context.h"
#include "queue.h"

int GVulkanProfileCmdBuffers = 0;
int GVulkanUseCmdBufferTimingForGPUTime = 0;
int GVulkanUploadCmdBufferSemaphore = 0;

CmdBuffer::CmdBuffer(Device *InDevice, CommandBufferPool *InCommandBufferPool, bool bInIsUploadOnly)
	: CurrentStencilRef(0),
	  State(EState::NotAllocated),
	  bNeedsDynamicStateSet(1),
	  bHasPipeline(0),
	  bHasViewport(0),
	  bHasScissor(0),
	  bHasStencilRef(0),
	  bIsUploadOnly(bInIsUploadOnly ? 1 : 0),
	  device(InDevice),
	  CommandBufferHandle(VK_NULL_HANDLE),
	  fence(nullptr),
	  commandBufferPool(InCommandBufferPool),
	  LayoutManager(device->SupportsParallelRendering(), &InCommandBufferPool->GetMgr().GetCommandListContext()->GetQueue()->GetLayoutManager())
{
	// FScopeLock ScopeLock(CommandBufferPool->GetCS());
	AllocMemory();
	fence = device->GetFenceManager().AllocateFence();
}

CmdBuffer::~CmdBuffer()
{
	VulkanRHI::FenceManager &FenceManager = device->GetFenceManager();
	if (State == EState::Submitted)
	{
		// Wait 33ms
		uint64 WaitForCmdBufferInNanoSeconds = 33 * 1000 * 1000LL;
		FenceManager.WaitAndReleaseFence(fence, WaitForCmdBufferInNanoSeconds);
	}
	else
	{
		// Just free the fence, CmdBuffer was not submitted
		FenceManager.ReleaseFence(fence);
	}

	if (State != EState::NotAllocated)
	{
		FreeMemory();
	}

	if (Timing)
	{
		assert(Timing == nullptr);
		// Timing->Release();
		// delete Timing;
		// Timing = nullptr;
	}
}

void CmdBuffer::AddWaitSemaphore(VkPipelineStageFlags InWaitFlags, std::vector<VulkanRHI::Semaphore *> InWaitSemaphores)
{
	WaitFlags.reserve(WaitFlags.size() + InWaitSemaphores.size());

	for (VulkanRHI::Semaphore *Sema : InWaitSemaphores)
	{
		WaitFlags.push_back(InWaitFlags);
		Sema->AddRef();

		bool Contains = false;
		for (auto s : WaitSemaphores)
		{
			if (s == Sema)
			{
				Contains = true;
				break;
			}
		}
		check(!Contains);

		WaitSemaphores.push_back(Sema);
	}
}

void CmdBuffer::Begin()
{
	{
		// FScopeLock ScopeLock(CommandBufferPool->GetCS());
		if (State == EState::NeedReset)
		{
			vkResetCommandBuffer(CommandBufferHandle, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
		}
		else
		{
			if (State == EState::ReadyForBegin)
				printf("Can't Begin as we're NOT ready! CmdBuffer 0x%p State=%d\n", CommandBufferHandle, (int32_t)State);
		}
		State = EState::IsInsideBegin;
	}

	VkCommandBufferBeginInfo CmdBufBeginInfo;
	ZeroVulkanStruct(CmdBufBeginInfo, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
	CmdBufBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(CommandBufferHandle, &CmdBufBeginInfo);

	if (GVulkanProfileCmdBuffers || GVulkanUseCmdBufferTimingForGPUTime)
	{
		printf("Fuck!! %s: %s\n", __FILE__, __LINE__);
		// InitializeTimings(CommandBufferPool->GetMgr().GetCommandListContext());
		// if (Timing)
		// {
		// 	Timing->StartTiming(this);
		// }
	}
	check(!CurrentDescriptorPoolSetContainer);

	if (!bIsUploadOnly && device->SupportsBindless())
	{
		printf("Fuck!! %s: %s\n", __FILE__, __LINE__);
		// FVulkanBindlessDescriptorManager *BindlessDescriptorManager = Device->GetBindlessDescriptorManager();
		// FVulkanQueue *Queue = GetOwner()->GetMgr().GetQueue();
		// const VkPipelineStageFlags SupportedStages = Queue->GetSupportedStageBits();
		// BindlessDescriptorManager->BindDescriptorBuffers(CommandBufferHandle, SupportedStages);
	}

	bNeedsDynamicStateSet = true;
}

void CmdBuffer::End()
{
	if (!IsOutsideRenderPass())
	{
		printf("Can't End as we're inside a render pass! CmdBuffer 0x%p State=%d\n", CommandBufferHandle, (int32_t)State);
	}
	// checkf(IsOutsideRenderPass(), TEXT("Can't End as we're inside a render pass! CmdBuffer 0x%p State=%d"), CommandBufferHandle, (int32)State);

	if (GVulkanProfileCmdBuffers || GVulkanUseCmdBufferTimingForGPUTime)
	{
		printf("Fuck!! %s: %s\n", __FILE__, __LINE__);
		// if (Timing)
		// {
		// 	Timing->EndTiming(this);
		// 	LastValidTiming = FenceSignaledCounter;
		// }
	}

	for (PendingQuery &Query : PendingTimestampQueries)
	{
		printf("Fuck!! %s: %s\n", __FILE__, __LINE__);
		break;
		const uint64 Index = Query.Index;
		const VkBuffer BufferHandle = Query.BufferHandle;
		const VkQueryPool PoolHandle = Query.PoolHandle;
		const VkQueryResultFlags BlockingFlags = Query.bBlocking ? VK_QUERY_RESULT_WAIT_BIT : VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
		const uint32 Width = (Query.bBlocking ? 1 : 2);
		const uint32 Stride = sizeof(uint64) * Width;

		vkCmdCopyQueryPoolResults(GetHandle(), PoolHandle, Index, Query.Count, BufferHandle, Stride * Index, Stride, VK_QUERY_RESULT_64_BIT | BlockingFlags);
		if (Query.bBlocking)
		{
			vkCmdResetQueryPool(GetHandle(), PoolHandle, Index, Query.Count);
		}
	}

	PendingTimestampQueries.clear();

	VERIFYVULKANRESULT(vkEndCommandBuffer(GetHandle()));
	State = EState::HasEnded;
}

void CmdBuffer::AllocMemory()
{
	VkCommandBufferAllocateInfo CreateCmdBufInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	CreateCmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	CreateCmdBufInfo.commandBufferCount = 1;
	CreateCmdBufInfo.commandPool = commandBufferPool->GetHandle();

	vkAllocateCommandBuffers(device->GetInstanceHandle(), &CreateCmdBufInfo, &CommandBufferHandle);
}

void CmdBuffer::FreeMemory()
{
	vkFreeCommandBuffers(device->GetInstanceHandle(), commandBufferPool->GetHandle(), 1, &CommandBufferHandle);
	CommandBufferHandle = VK_NULL_HANDLE;
}

void CmdBuffer::EndRenderPass()
{
	if (!IsInsideRenderPass())
		printf("Can't EndRP as we're NOT inside one! CmdBuffer 0x%p State=%d", CommandBufferHandle, (int32)State);
	vkCmdEndRenderPass(CommandBufferHandle);
	State = EState::IsInsideBegin;
}

void CmdBuffer::RefreshFenceStatus()
{
	if (State == EState::Submitted)
	{
		VulkanRHI::FenceManager *fenceMgr = fence->GetOwner();
		if (fenceMgr->IsFenceSignaled(fence))
		{
			bHasPipeline = false;
			bHasViewport = false;
			bHasScissor = false;
			bHasStencilRef = false;

			for (VulkanRHI::Semaphore *Semaphore : SubmittedWaitSemaphores)
			{
				Semaphore->Release();
			}
			SubmittedWaitSemaphores.clear();

			CurrentViewports.clear();
			CurrentScissors.clear();
			CurrentStencilRef = 0;
#if VULKAN_REUSE_FENCES
			fence->GetOwner()->ResetFence(fence);
#else
			VulkanRHI::Fence *PrevFence = fence;
			fence = fenceMgr->AllocateFence();
			fenceMgr->ReleaseFence(PrevFence);
#endif
			++FenceSignaledCounter;

			if (CurrentDescriptorPoolSetContainer)
			{
				// #todo-rco: Reset here?
				TypedDescriptorPoolSets.clear();
				device->GetDescriptorPoolsManager().ReleasePoolSet(*CurrentDescriptorPoolSetContainer);
				CurrentDescriptorPoolSetContainer = nullptr;
			}
			else
			{
				check(TypedDescriptorPoolSets.size() == 0);
			}

			// Change state at the end to be safe
			State = EState::NeedReset;
		}
	}
	else
	{
		check(!fence->IsSignaled());
	}
}

CommandBufferPool::~CommandBufferPool()
{
	for (int32 Index = 0; Index < CmdBuffers.size(); ++Index)
	{
		CmdBuffer *cmdBuffer = CmdBuffers[Index];
		delete cmdBuffer;
	}

	for (int32 Index = 0; Index < FreeCmdBuffers.size(); ++Index)
	{
		CmdBuffer *cmdBuffer = FreeCmdBuffers[Index];
		delete cmdBuffer;
	}

	vkDestroyCommandPool(Device->GetInstanceHandle(), Handle, VULKAN_CPU_ALLOCATOR);
	Handle = VK_NULL_HANDLE;
}

void CommandBufferPool::RefreshFenceStatus(CmdBuffer *SkipCmdBuffer)
{
	// FScopeLock ScopeLock(&CS);
	for (int32 Index = 0; Index < CmdBuffers.size(); ++Index)
	{
		CmdBuffer *CmdBuffer = CmdBuffers[Index];
		if (CmdBuffer != SkipCmdBuffer)
		{
			CmdBuffer->RefreshFenceStatus();
		}
	}
}

CmdBuffer *CommandBufferPool::Create(bool bIsUploadOnly)
{
	// Assumes we are inside a lock for the pool
	for (int32 Index = FreeCmdBuffers.size() - 1; Index >= 0; --Index)
	{
		CmdBuffer *cmdBuffer = FreeCmdBuffers[Index];
#if VULKAN_USE_DIFFERENT_POOL_CMDBUFFERS
		if (cmdBuffer->bIsUploadOnly == bIsUploadOnly)
#endif
		{
			CmdBuffer *back = FreeCmdBuffers.back();
			FreeCmdBuffers.pop_back();
			FreeCmdBuffers[Index] = back;
			// FreeCmdBuffers.RemoveAtSwap(Index);
			cmdBuffer->AllocMemory();
			CmdBuffers.push_back(cmdBuffer);
			return cmdBuffer;
		}
	}

	CmdBuffer *cmdBuffer = new CmdBuffer(Device, this, bIsUploadOnly);
	CmdBuffers.push_back(cmdBuffer);
	check(cmdBuffer);
	return cmdBuffer;
}

void CommandBufferPool::Create(uint32 QueueFamilyIndex)
{
	VkCommandPoolCreateInfo CmdPoolInfo;
	ZeroVulkanStruct(CmdPoolInfo, VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
	CmdPoolInfo.queueFamilyIndex = QueueFamilyIndex;
	// #todo-rco: Should we use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT?
	CmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VERIFYVULKANRESULT(vkCreateCommandPool(Device->GetInstanceHandle(), &CmdPoolInfo, VULKAN_CPU_ALLOCATOR, &Handle));
}

CommandBufferManager::CommandBufferManager(Device *InDevice, CommandListContext *InContext)
	: device(InDevice), context(InContext), pool(InDevice, *this), queue(InContext->GetQueue()),
	  activeCmdBuffer(nullptr), uploadCmdBuffer(nullptr)
{
	check(device);
	pool.Create(queue->GetFamilyIndex());

	activeCmdBuffer = pool.Create(false);
}

void CommandBufferManager::Init(CommandListContext *InContext)
{
	// ActiveCmdBuffer->InitializeTimings(InContext);
	activeCmdBuffer->Begin();
}

CmdBuffer *CommandBufferManager::GetUploadCmdBuffer()
{
	if (!uploadCmdBuffer)
	{
		for (int32_t Index = 0; Index < pool.CmdBuffers.size(); ++Index)
		{
			CmdBuffer *CmdBuffer = pool.CmdBuffers[Index];
			CmdBuffer->RefreshFenceStatus();
			{
				if (CmdBuffer->State == CmdBuffer::EState::ReadyForBegin || CmdBuffer->State == CmdBuffer::EState::NeedReset)
				{
					uploadCmdBuffer = CmdBuffer;
					uploadCmdBuffer->Begin();
					return uploadCmdBuffer;
				}
			}
		}

		// All cmd buffers are being executed still
		uploadCmdBuffer = pool.Create(true);
		uploadCmdBuffer->Begin();
	}
	return uploadCmdBuffer;
}

void CommandBufferManager::SubmitUploadCmdBuffer(uint32_t NumSignalSemaphores, VkSemaphore *SignalSemaphores)
{
	// FScopeLock ScopeLock(&Pool.CS);
	check(uploadCmdBuffer);
	check(uploadCmdBuffer->CurrentDescriptorPoolSetContainer == nullptr);
	if (!uploadCmdBuffer->IsSubmitted() && uploadCmdBuffer->HasBegun())
	{
		check(uploadCmdBuffer->IsOutsideRenderPass());

// VulkanRHI::DebugHeavyWeightBarrier(UploadCmdBuffer->GetHandle(), 4);
#ifdef PRINT_UNIMPLEMENT
		printf("DebugHeavyWeightBarrier %s %d\n", __FILE__, __LINE__);
#endif

		uploadCmdBuffer->End();

		if (0)
		{
			// // Add semaphores associated with the recent active cmdbuf(s), if any. That will prevent
			// // the overlap, delaying execution of this cmdbuf until the graphics one(s) is complete.
			// for (FSemaphore *WaitForThis : RenderingCompletedSemaphores)
			// {
			// 	UploadCmdBuffer->AddWaitSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, WaitForThis);
			// }

			// if (NumSignalSemaphores == 0)
			// {
			// 	checkf(UploadCmdBufferSemaphore != nullptr, TEXT("FVulkanCommandBufferManager::SubmitUploadCmdBuffer: upload command buffer does not have an associated completion semaphore."));
			// 	VkSemaphore Sema = UploadCmdBufferSemaphore->GetHandle();
			// 	Queue->Submit(UploadCmdBuffer, 1, &Sema);
			// 	UploadCompletedSemaphores.Add(UploadCmdBufferSemaphore);
			// 	UploadCmdBufferSemaphore = nullptr;
			// }
			// else
			// {
			// 	TArray<VkSemaphore, TInlineAllocator<16>> CombinedSemaphores;
			// 	CombinedSemaphores.Add(UploadCmdBufferSemaphore->GetHandle());
			// 	CombinedSemaphores.Append(SignalSemaphores, NumSignalSemaphores);
			// 	Queue->Submit(UploadCmdBuffer, CombinedSemaphores.Num(), CombinedSemaphores.GetData());
			// }
			// // the buffer will now hold on to the wait semaphores, so we can clear them here
			// RenderingCompletedSemaphores.Empty();
		}
		else
		{
			queue->Submit(uploadCmdBuffer, NumSignalSemaphores, SignalSemaphores);
		}
	}

	uploadCmdBuffer = nullptr;
}

void CommandBufferManager::SubmitActiveCmdBuffer(std::vector<VulkanRHI::Semaphore *> SignalSemaphores)
{
	// FScopeLock ScopeLock(&Pool.CS);
	FlushResetQueryPools();
	check(!uploadCmdBuffer);
	check(activeCmdBuffer);

	std::vector<VkSemaphore> SemaphoreHandles;
	SemaphoreHandles.reserve(SignalSemaphores.size() + 1);
	for (VulkanRHI::Semaphore *Semaphore : SignalSemaphores)
	{
		SemaphoreHandles.push_back(Semaphore->GetHandle());
	}

	if (!activeCmdBuffer->IsSubmitted() && activeCmdBuffer->HasBegun())
	{
		if (!activeCmdBuffer->IsOutsideRenderPass())
		{
			printf("Should Forcing EndRenderPass() for submission %s %d\n", __FILE__, __LINE__);
			// UE_LOG(LogVulkanRHI, Warning, TEXT("Forcing EndRenderPass() for submission"));
			activeCmdBuffer->EndRenderPass();
		}
#ifdef PRINT_UNIMPLEMENT
		printf("DebugHeavyWeightBarrier %s %d\n", __FILE__, __LINE__);
#endif
		// VulkanRHI::DebugHeavyWeightBarrier(activeCmdBuffer->GetHandle(), 8);

		activeCmdBuffer->End();

		queue->Submit(activeCmdBuffer, SemaphoreHandles.size(), SemaphoreHandles.data());
	}

	activeCmdBuffer = nullptr;
}

void CommandBufferManager::SubmitActiveCmdBufferFromPresent(VulkanRHI::Semaphore *SignalSemaphore)
{
	if (GVulkanUploadCmdBufferSemaphore)
	{
		printf("ERROR %s %d\n", __FILE__, __LINE__);
		// // unlike more advanced regular SACB(), this is just a wrapper
		// // around Queue->Submit() to avoid rewriting the logic in Present
		// if (SignalSemaphore)
		// {
		// 	VkSemaphore SignalThis[2] =
		// 		{
		// 			SignalSemaphore->GetHandle(),
		// 			activeCmdBufferSemaphore->GetHandle()};

		// 	queue->Submit(ActiveCmdBuffer, UE_ARRAY_COUNT(SignalThis), SignalThis);
		// }
		// else
		// {
		// 	VkSemaphore SignalThis = ActiveCmdBufferSemaphore->GetHandle();
		// 	queue->Submit(ActiveCmdBuffer, 1, &SignalThis);
		// }

		// RenderingCompletedSemaphores.Add(ActiveCmdBufferSemaphore);
		// ActiveCmdBufferSemaphore = nullptr;
	}
	else
	{
		if (SignalSemaphore)
		{
			queue->Submit(activeCmdBuffer, SignalSemaphore->GetHandle());
		}
		else
		{
			queue->Submit(activeCmdBuffer);
		}
	}
}

void CommandBufferManager::WaitForCmdBuffer(CmdBuffer *cmdBuffer, float timeInSecondsToWait)
{
	check(cmdBuffer->IsSubmitted());
	bool bSuccess = device->GetFenceManager().WaitForFence(cmdBuffer->fence, (uint64)(timeInSecondsToWait * 1e9));
	check(bSuccess);
	cmdBuffer->RefreshFenceStatus();
}

void CommandBufferManager::FlushResetQueryPools()
{
#ifdef PRINT_UNIMPLEMENT
	printf("Have not implemented CommandBufferManager::FlushResetQueryPools\n");
#endif
}

void CommandBufferManager::PrepareForNewActiveCommandBuffer()
{
	check(!uploadCmdBuffer);

	for (int32 Index = 0; Index < pool.CmdBuffers.size(); ++Index)
	{
		CmdBuffer *CmdBuffer = pool.CmdBuffers[Index];
		CmdBuffer->RefreshFenceStatus();
#if VULKAN_USE_DIFFERENT_POOL_CMDBUFFERS
		if (!CmdBuffer->bIsUploadOnly)
#endif
		{
			if (CmdBuffer->State == CmdBuffer::EState::ReadyForBegin || CmdBuffer->State == CmdBuffer::EState::NeedReset)
			{
				activeCmdBuffer = CmdBuffer;
				activeCmdBuffer->Begin();
				return;
			}
			else
			{
				check(CmdBuffer->State == CmdBuffer::EState::Submitted);
			}
		}
	}

	// All cmd buffers are being executed still
	activeCmdBuffer = pool.Create(false);
	activeCmdBuffer->Begin();
}