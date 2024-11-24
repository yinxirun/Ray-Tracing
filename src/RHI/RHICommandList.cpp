#include "RHICommandList.h"
#include "core/assertion_macros.h"
#include "core/enum_class_flags.h"
#include "RHI/RHIContext.h"
#include "RHI/dynamic_rhi.h"
#include <cmath>
#include <atomic>

bool GIsRunningRHIInSeparateThread_InternalUseOnly = false;

RHICommandListExecutor GRHICommandListExecutor;

template <typename T>
static constexpr __forceinline bool IsPowerOfTwo(T Value)
{
	return ((Value & (Value - 1)) == (T)0);
}

RHICommandListBase::RHICommandListBase(RHIGPUMask InGPUMask, ERecordingThread InRecordingThread, bool bImmediate)
	: RHICommandListBase(FPersistentState(InGPUMask, InRecordingThread, bImmediate))
{
}

bool RHICommandListBase::Bypass() const
{
	check(!IsImmediate() || IsInRenderingThread() || IsInRHIThread());
	return GRHICommandListExecutor.Bypass() && IsImmediate();
}

RHIPipeline RHICommandListBase::SwitchPipeline(RHIPipeline Pipeline)
{
	RHIPipeline allowedPipeline = RHIPipeline::All;

	check(Pipeline == RHIPipeline::None || IsPowerOfTwo((__underlying_type(RHIPipeline))Pipeline));
	(Pipeline == RHIPipeline::None || EnumHasAnyFlags(allowedPipeline, Pipeline));

	std::swap(activePipeline, Pipeline);
	if (activePipeline != Pipeline)
	{
		EnqueueLambda([NewPipeline = activePipeline](RHICommandListBase &ExecutingCmdList)
					  {
			ExecutingCmdList.activePipeline = NewPipeline;

			//
			// Grab the appropriate command contexts from the RHI if we don't already have them.
			// Also update the GraphicsContext/ComputeContext pointers to direct recorded commands
			// to the correct target context, based on which pipeline is now active.
			//
			if (NewPipeline == RHIPipeline::None)
			{
				ExecutingCmdList.graphicsContext = nullptr;
				ExecutingCmdList.computeContext  = nullptr;
			}
			else
			{
				ComputeContext*& context = ExecutingCmdList.Contexts[0];

				switch (NewPipeline)
				{
				default: checkNoEntry();
				case RHIPipeline::Graphics:
					{
						if (!context)
						{
							// Need to handle the "immediate" context separately.
							context = ExecutingCmdList.persistentState.bImmediate
								? ::GetDefaultContext()
								: rhi->GetCommandContext(NewPipeline, RHIGPUMask::All()); // This mask argument specifies which contexts are included in an mGPU redirector (we always want all of them).
						}

						ExecutingCmdList.graphicsContext = static_cast<CommandContext*>(context);
						ExecutingCmdList.computeContext  = context;
					}
					break;

				case RHIPipeline::AsyncCompute:
					{
						if (!context)
						{
							context = rhi->GetCommandContext(NewPipeline, RHIGPUMask::All()); // This mask argument specifies which contexts are included in an mGPU redirector (we always want all of them).
							check(context);
						}

						ExecutingCmdList.graphicsContext = nullptr;
						ExecutingCmdList.computeContext  = context;
					}
					break;
				}

				// (Re-)apply the current GPU mask.
				context->SetGPUMask(ExecutingCmdList.persistentState.currentGPUMask);

				/* // Ensure the context has an up-to-date stat pointer.
				ExecutingCmdList.persistentState.Stats.ApplyToContext(context); */
			} });
	}

	return Pipeline;
}

RHICommandListBase::RHICommandListBase(FPersistentState &&InPersistentState)
	: /* DispatchEvent(FGraphEvent::CreateGraphEvent()), */ persistentState(std::move(InPersistentState))
{
	UID = GRHICommandListExecutor.UIDCounter.load();
	GRHICommandListExecutor.UIDCounter++;
}
