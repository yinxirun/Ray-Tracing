#pragma once

#include "resources.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include "../definitions.h"

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
    BackBuffer(Device &Device, Viewport *InViewport, VkFormat Format, uint32_t SizeX, uint32_t SizeY, ETextureCreateFlags UEFlags);

    virtual ~BackBuffer();

    void ReleaseAcquiredImage();

private:
    Viewport *viewport;
};

class Viewport : public Resource
{
public:
    enum
    {
        NUM_BUFFERS = 3
    };
    Viewport(Device *d, void *InWindowHandle, uint32_t sizeX, uint32_t sizeY, EPixelFormat InPreferredPixelFormat);

    ~Viewport();

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
    std::shared_ptr<Texture> RenderingBackBuffer;

    uint32_t sizeX;
    uint32_t sizeY;
    EPixelFormat pixelFormat;
    int32 acquiredImageIndex;
    SwapChain *swapChain;
    void *windowHandle;
    uint32 presentCount;
    bool bRenderOffscreen;

    // Just a pointer, not owned by this class
	VulkanRHI::Semaphore* acquiredSemaphore;

    void CreateSwapchain(struct SwapChainRecreateInfo *RecreateInfo);
    void DestroySwapchain(struct SwapChainRecreateInfo *RecreateInfo);
    bool TryAcquireImageIndex();

    void RecreateSwapchain(void* NewNativeWindow);

    bool DoCheckedSwapChainJob(std::function<int32(Viewport*)> SwapChainJob);

    bool SupportsStandardSwapchain();

    EPixelFormat GetPixelFormatForNonDefaultSwapchain();
};