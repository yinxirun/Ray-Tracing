#pragma once

#include "resources.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include "../definitions.h"
#include "gpu/RHI/RHICommandList.h"

class Device;
class SwapChain;
class View;
class Viewport;
class Queue;
class CommandListContext;
class CmdBuffer;

namespace VulkanRHI
{
    class Semaphore;
}

class BackBuffer : public Texture
{
public:
    BackBuffer(Device &Device, Viewport *InViewport, EPixelFormat Format, uint32_t SizeX, uint32_t SizeY, ETextureCreateFlags Flags);

    virtual ~BackBuffer();

    void OnGetBackBufferImage(RHICommandListImmediate &RHICmdList);

    void ReleaseViewport();
    void ReleaseAcquiredImage();

private:
    void AcquireBackBufferImage(CommandListContext &Context);
    Viewport *viewport;
};

class Viewport : public RHIResource
{
public:
    enum
    {
        NUM_BUFFERS = 3
    };
    Viewport(Device *d, void *InWindowHandle, uint32_t sizeX, uint32_t sizeY, EPixelFormat InPreferredPixelFormat);
    ~Viewport();
    std::shared_ptr<Texture> GetBackBuffer(RHICommandListImmediate &RHICmdList);

    virtual void WaitForFrameEventCompletion();

    virtual void IssueFrameEvent();

    bool Present(CommandListContext *context, CmdBuffer *cmdBuffer, Queue *queue, Queue *presentQueue, bool bLockToVsync);

protected:
    Device *device;

    std::vector<VkImage> backBufferImages;
    std::vector<VulkanRHI::Semaphore *> renderingDoneSemaphores;
    std::vector<View *> textureViews;
    std::shared_ptr<BackBuffer> RHIBackBuffer;
    // 'Dummy' back buffer
    std::shared_ptr<Texture> renderingBackBuffer;

    uint32_t sizeX;
    uint32_t sizeY;
    EPixelFormat pixelFormat;
    int32 acquiredImageIndex;
    SwapChain *swapChain;
    void *windowHandle;
    bool bRenderOffscreen;

    // Just a pointer, not owned by this class
    VulkanRHI::Semaphore *acquiredSemaphore;

    CmdBuffer *LastFrameCommandBuffer = nullptr;
    uint64 LastFrameFenceCounter = 0;

    void CreateSwapchain(struct SwapChainRecreateInfo *RecreateInfo);
    void DestroySwapchain(struct SwapChainRecreateInfo *RecreateInfo);
    bool TryAcquireImageIndex();

    void RecreateSwapchain(void *);

    bool DoCheckedSwapChainJob(std::function<int32(Viewport *)> SwapChainJob);
    bool SupportsStandardSwapchain();
    bool RequiresRenderingBackBuffer();

    EPixelFormat GetPixelFormatForNonDefaultSwapchain();

    friend class BackBuffer;
};