#pragma once

class CommandListContext;

class RHICommandListBase
{
public:
    // The values in this struct are preserved when the command list is moved or reset.
    struct FPersistentState
    {
        bool bImmediate = false;
    } persistentState;

    inline bool IsImmediate() const
    {
        return persistentState.bImmediate;
    }

    inline CommandListContext &GetContext() { return *GraphicsContext; }

protected:
    // The active context into which graphics commands are recorded.
    CommandListContext *GraphicsContext = nullptr;

    // The active compute context into which (possibly async) compute commands are recorded.
    // RHIComputeContext *ComputeContext = nullptr;
};

class RHIComputeCommandList : public RHICommandListBase
{
};

class RHICommandList : public RHIComputeCommandList
{
};

class RHICommandListImmediate : public RHICommandList
{
};