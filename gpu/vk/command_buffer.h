#pragma once

#include "Volk/volk.h"
#include "device.h"
#include "barrier.h"
#include <map>

class Device;
class CommandListContext;
class CommandBufferManager;
class LayoutManager;
class CommandBufferPool;
class DescriptorPoolSetContainer;
class RenderTargetLayout;

namespace VulkanRHI
{
    class Semaphore;
    class Fence;
}

class CmdBuffer
{
protected:
    friend class CommandBufferManager;
    friend class CommandBufferPool;
    friend class Queue;

    CmdBuffer(Device *InDevice, CommandBufferPool *InCommandBufferPool, bool bInIsUploadOnly);
    ~CmdBuffer();

public:
    CommandBufferPool *GetOwner()
    {
        return commandBufferPool;
    }

    Device *GetDevice() { return device; }

    inline bool IsInsideRenderPass() const { return State == EState::IsInsideRenderPass; }
    inline bool IsOutsideRenderPass() const { return State == EState::IsInsideBegin; }

    inline bool HasBegun() const { return State == EState::IsInsideBegin || State == EState::IsInsideRenderPass; }
    inline bool HasEnded() const { return State == EState::HasEnded; }
    inline bool IsSubmitted() const { return State == EState::Submitted; }

    inline VkCommandBuffer GetHandle() { return CommandBufferHandle; }

    inline volatile uint64 GetFenceSignaledCounter() const { return FenceSignaledCounter; }

    inline volatile uint64 GetSubmittedFenceCounter() const { return SubmittedFenceCounter; }

    inline LayoutManager &GetLayoutManager() { return LayoutManager; }

    void AddWaitSemaphore(VkPipelineStageFlags InWaitFlags, VulkanRHI::Semaphore *InWaitSemaphore)
    {
        std::vector<VulkanRHI::Semaphore *> waitSemaView = {InWaitSemaphore};
        AddWaitSemaphore(InWaitFlags, waitSemaView);
    }

    void AddWaitSemaphore(VkPipelineStageFlags InWaitFlags, std::vector<VulkanRHI::Semaphore *> InWaitSemaphores);

    void Begin();
    void End();

    enum class EState : uint8_t
    {
        ReadyForBegin,
        IsInsideBegin,
        IsInsideRenderPass,
        HasEnded,
        Submitted,
        NotAllocated,
        NeedReset,
    };

    std::vector<VkViewport> CurrentViewports;
    std::vector<VkRect2D> CurrentScissors;
    uint32 CurrentStencilRef;
    EState State;
    uint8 bNeedsDynamicStateSet : 1;
    uint8 bHasPipeline : 1;
    uint8 bHasViewport : 1;
    uint8 bHasScissor : 1;
    uint8 bHasStencilRef : 1;
    uint8 bIsUploadOnly : 1;
    uint8 bIsUniformBufferBarrierAdded : 1;

    // You never want to call Begin/EndRenderPass directly as it will mess up the layout manager.
    void BeginRenderPass(const RenderTargetLayout &Layout, class RenderPass *RenderPass,
                         class Framebuffer *Framebuffer, const VkClearValue *AttachmentClearValues);
    void EndRenderPass();

    void BeginUniformUpdateBarrier();
    void EndUniformUpdateBarrier();

    // #todo-rco: Hide this
    DescriptorPoolSetContainer *CurrentDescriptorPoolSetContainer = nullptr;

    struct PendingQuery
    {
        uint64_t Index;
        uint64_t Count;
        VkBuffer BufferHandle;
        VkQueryPool PoolHandle;
        bool bBlocking;
    };

private:
    Device *device;
    VkCommandBuffer CommandBufferHandle;
    double SubmittedTime = 0.0f;

    std::vector<VkPipelineStageFlags> WaitFlags;
    std::vector<VulkanRHI::Semaphore *> WaitSemaphores;
    std::vector<VulkanRHI::Semaphore *> SubmittedWaitSemaphores;
    std::vector<PendingQuery> PendingTimestampQueries;

    void MarkSemaphoresAsSubmitted()
    {
        WaitFlags.clear();
        // Move to pending delete list
        SubmittedWaitSemaphores = WaitSemaphores;
        WaitSemaphores.clear();
    }

    // Do not cache this pointer as it might change depending on VULKAN_REUSE_FENCES
    VulkanRHI::Fence *fence;

    // Last value passed after the fence got signaled
    volatile uint64_t FenceSignaledCounter;
    // Last value when we submitted the cmd buffer; useful to track down if something waiting for the fence has actually been submitted
    volatile uint64_t SubmittedFenceCounter;

    void RefreshFenceStatus();

    CommandBufferPool *commandBufferPool;

    void *Timing = nullptr;

    void AcquirePoolSetContainer();

    void AllocMemory();
    void FreeMemory();

    LayoutManager LayoutManager;

public:
    // #todo-rco: Hide this
    std::map<uint32_t, class TypedDescriptorPoolSet *> TypedDescriptorPoolSets;
};

class CommandBufferPool
{
public:
    CommandBufferPool(Device *device, CommandBufferManager &mgr) : Handle(VK_NULL_HANDLE), Device(device), Mgr(mgr) {}

    ~CommandBufferPool();

    void RefreshFenceStatus(CmdBuffer *SkipCmdBuffer = nullptr);

    VkCommandPool GetHandle() const { return Handle; }

    void FreeUnusedCmdBuffers(Queue *Queue, bool bTrimMemory);

    inline CommandBufferManager &GetMgr() { return Mgr; }

private:
    VkCommandPool Handle;

    std::vector<CmdBuffer *> CmdBuffers;
    std::vector<CmdBuffer *> FreeCmdBuffers;

    Device *Device;
    CommandBufferManager &Mgr;

    CmdBuffer *Create(bool bIsUploadOnly);
    void Create(uint32 QueueFamilyIndex);
    friend class CommandBufferManager;
};

/// @brief 一个CommandBufferManager对应着一个VkCommandPool
class CommandBufferManager
{
public:
    CommandBufferManager(Device *InDevice, CommandListContext *InContext);

    void Init(CommandListContext *InContext);

    inline CmdBuffer *GetActiveCmdBuffer()
    {
        if (uploadCmdBuffer)
        {
            SubmitUploadCmdBuffer();
        }

        return activeCmdBuffer;
    }

    inline CmdBuffer *GetActiveCmdBufferDirect() { return activeCmdBuffer; }

    inline bool HasPendingUploadCmdBuffer() const { return uploadCmdBuffer != nullptr; }

    inline bool HasPendingActiveCmdBuffer() const { return activeCmdBuffer != nullptr; }

    CmdBuffer *GetUploadCmdBuffer();

    void SubmitUploadCmdBuffer(uint32_t NumSignalSemaphores = 0, VkSemaphore *SignalSemaphores = nullptr);

    void SubmitActiveCmdBuffer(std::vector<VulkanRHI::Semaphore *> SignalSemaphores);

    void SubmitActiveCmdBuffer()
    {
        SubmitActiveCmdBuffer(std::vector<VulkanRHI::Semaphore *>());
    }

    /** Regular SACB() expects not-ended and would rotate the command buffer immediately, but Present has a special logic */
    void SubmitActiveCmdBufferFromPresent(VulkanRHI::Semaphore *SignalSemaphore = nullptr);

    void WaitForCmdBuffer(CmdBuffer *CmdBuffer, float TimeInSecondsToWait = 10.0f);

    void FlushResetQueryPools();

    // Update the fences of all cmd buffers except SkipCmdBuffer
    void RefreshFenceStatus(CmdBuffer *SkipCmdBuffer = nullptr) { pool.RefreshFenceStatus(SkipCmdBuffer); }

    void PrepareForNewActiveCommandBuffer();

    inline VkCommandPool GetHandle() const { return pool.GetHandle(); }

    void FreeUnusedCmdBuffers(bool bTrimMemory);

    inline CommandListContext *GetCommandListContext() { return context; }

    inline void NotifyDeletedImage(VkImage Image)
    {
        if (uploadCmdBuffer)
        {
            uploadCmdBuffer->GetLayoutManager().NotifyDeletedImage(Image);
        }
        if (activeCmdBuffer)
        {
            activeCmdBuffer->GetLayoutManager().NotifyDeletedImage(Image);
        }
    }

private:
    Device *device;
    CommandListContext *context;
    CommandBufferPool pool;
    Queue *queue;
    CmdBuffer *activeCmdBuffer;
    CmdBuffer *uploadCmdBuffer;

    /** This semaphore is used to prevent overlaps between (current) upload cmdbuf and next graphics cmdbuf. */
    VulkanRHI::Semaphore *UploadCmdBufferSemaphore;
};