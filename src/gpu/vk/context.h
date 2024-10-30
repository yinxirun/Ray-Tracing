/// @file 一个 CommandListContext 对应着一个 CommandBufferManager，也对应着一个 CommandBufferPool 。这三者一一对应。\
/// 
#pragma once
#include <memory>

class RHI;
class Device;
class Queue;
class CommandBufferManager;
class Viewport;

class CommandListContext
{
public:
    CommandListContext(RHI *InRHI, Device *InDevice, Queue *InQueue, CommandListContext *InImmediate);
    virtual ~CommandListContext();

    inline bool IsImmediate() const { return Immediate == nullptr; }
    
    // RHI
    virtual void RHIPushEvent(const char* Name, int Color) final;
    virtual void RHIPopEvent() final;

    virtual void RHIBeginDrawingViewport(std::shared_ptr<Viewport> &Viewport) final;
    virtual void RHIEndDrawingViewport(Viewport* Viewport, bool bLockToVsync) final;

    inline CommandBufferManager *GetCommandBufferManager() { return commandBufferManager; }

    inline Queue *GetQueue() { return queue; }

protected:
    RHI *rhi;
    CommandListContext *Immediate;
    Device *device;
    CommandBufferManager *commandBufferManager;
    Queue *queue;
};

class CommandListContextImmediate : public CommandListContext
{
public:
    CommandListContextImmediate(RHI *InRHI, Device *InDevice, Queue *InQueue);
};