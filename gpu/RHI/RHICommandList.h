#pragma once
#include <memory>
#include <array>
#include <atomic>
#include "gpu/definitions.h"
#include "gpu/core/assertion_macros.h"
#include "gpu/RHI/multi_gpu.h"
#include "gpu/RHI/RHIPipeline.h"
class ComputeContext;
class CommandContext;

extern bool GIsRunningRHIInSeparateThread_InternalUseOnly;

/// Whether the RHI commands are being run in a thread other than the render thread
bool __forceinline IsRunningRHIInSeparateThread() { return GIsRunningRHIInSeparateThread_InternalUseOnly; }

class RHICommandListBase
{
public:
    enum class ERecordingThread
    {
        Render,
        Any
    };

protected:
    RHICommandListBase(RHIGPUMask InGPUMask, ERecordingThread InRecordingThread, bool bInImmediate);

public:
    // The values in this struct are preserved when the command list is moved or reset.
    struct FPersistentState
    {
        bool bInsideRenderPass = false;
        bool bInsideComputePass = false;
        bool bImmediate = false;
        RHIGPUMask currentGPUMask;
        ERecordingThread RecordingThread;
        RHIGPUMask CurrentGPUMask;
        RHIGPUMask InitialGPUMask;

        FPersistentState(RHIGPUMask InInitialGPUMask, ERecordingThread InRecordingThread, bool bInImmediate = false)
            : bImmediate(bInImmediate), RecordingThread(InRecordingThread), CurrentGPUMask(InInitialGPUMask), InitialGPUMask(InInitialGPUMask) {}
    } persistentState;

    inline bool IsImmediate() const { return persistentState.bImmediate; }

    template <typename LAMBDA>
    inline void EnqueueLambda(const char *LambdaName, LAMBDA &&Lambda)
    {
        if (IsBottomOfPipe())
        {
            Lambda(*this);
        }
        else
        {
            check(0);
            /* ALLOC_COMMAND(TRHILambdaCommand<FRHICommandListBase, LAMBDA>)
            (Forward<LAMBDA>(Lambda), LambdaName); */
        }
    }

    template <typename LAMBDA>
    inline void EnqueueLambda(LAMBDA &&Lambda)
    {
        RHICommandListBase::EnqueueLambda("TRHILambdaCommand", std::forward<LAMBDA>(Lambda));
    }

    __forceinline bool IsExecuting() const { return bExecuting; }

    __forceinline bool IsBottomOfPipe() const { return Bypass() || IsExecuting(); }
    __forceinline bool IsTopOfPipe() const { return !IsBottomOfPipe(); }

    inline CommandContext &GetContext()
    {
        check(graphicsContext);
        return *graphicsContext;
    }

    ComputeContext &GetComputeContext()
    {
        check(computeContext);
        return *computeContext;
    }

    bool Bypass() const;

    RHIPipeline SwitchPipeline(RHIPipeline Pipeline);
    bool IsOutsideRenderPass() const { return !persistentState.bInsideRenderPass; }
    bool IsInsideRenderPass() const { return persistentState.bInsideRenderPass; }
    bool IsInsideComputePass() const { return persistentState.bInsideComputePass; }

protected:
    // The active context into which graphics commands are recorded.
    CommandContext *graphicsContext = nullptr;

    // The active compute context into which (possibly async) compute commands are recorded.
    ComputeContext *computeContext = nullptr;

    // The RHI contexts available to the command list during execution.
    // These are always set for the immediate command list, see InitializeImmediateContexts().
    std::array<ComputeContext *, (size_t)RHIPipeline::Num> Contexts = {};

    uint32 UID = UINT32_MAX;
    bool bExecuting = false;

    // The currently selected pipeline that RHI commands are directed to, during command list recording.
    // This is also adjusted during command list execution based on recorded use of SwitchPipeline().
    RHIPipeline activePipeline = RHIPipeline::None;

private:
    RHICommandListBase(FPersistentState &&InPersistentState);
};

class RHIComputeCommandList : public RHICommandListBase
{
protected:
    RHIComputeCommandList(RHIGPUMask GPUMask, ERecordingThread InRecordingThread, bool bImmediate)
        : RHICommandListBase(GPUMask, InRecordingThread, bImmediate)
    {
    }
};

class RHICommandList : public RHIComputeCommandList
{
protected:
    RHICommandList(RHIGPUMask GPUMask, ERecordingThread InRecordingThread, bool bImmediate)
        : RHIComputeCommandList(GPUMask, InRecordingThread, bImmediate)
    {
    }
};

class RHICommandListImmediate : public RHICommandList
{
    friend class RHICommandListExecutor;
    RHICommandListImmediate()
        : RHICommandList(RHIGPUMask::All(), ERecordingThread::Render, true)
    {
    }
};

class RHICommandListExecutor
{
public:
    RHICommandListExecutor()
        : bLatchedBypass(true) /* , bLatchedUseParallelAlgorithms(false) */
    {
    }
    static inline RHICommandListImmediate &GetImmediateCommandList();
    inline bool Bypass() const { return bLatchedBypass; }

private:
    bool bLatchedBypass;
    friend class RHICommandListBase;
    std::atomic_int UIDCounter{0};
    RHICommandListImmediate CommandListImmediate;
};

extern RHICommandListExecutor GRHICommandListExecutor;
inline RHICommandListImmediate &RHICommandListExecutor::GetImmediateCommandList()
{
    // @todo - fix remaining use of the immediate command list on other threads, then uncomment this check.
    // check(IsInRenderingThread());
    return GRHICommandListExecutor.CommandListImmediate;
}