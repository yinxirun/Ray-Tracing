#pragma once

#include "resources.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include "../definitions.h"
#include "RHI/RHICommandList.h"
#include "core/math/vec.h"

class Device;
class SwapChain;
class VulkanView;
class VulkanViewport;
class Queue;
class CommandListContext;
class CmdBuffer;

namespace VulkanRHI
{
    class Semaphore;
}

class BackBuffer : public VulkanTexture
{
public:
    BackBuffer(Device &Device, VulkanViewport *InViewport, PixelFormat Format, uint32_t SizeX, uint32_t SizeY, TextureCreateFlags Flags);

    virtual ~BackBuffer();

    void OnGetBackBufferImage(RHICommandListImmediate &RHICmdList);

    void ReleaseViewport();
    void ReleaseAcquiredImage();

private:
    void AcquireBackBufferImage(CommandListContext &Context);
    VulkanViewport *viewport;
};

class VulkanViewport : public Viewport, public VulkanRHI::DeviceChild
{
public:
    enum
    {
        NUM_BUFFERS = 3
    };
    VulkanViewport(Device *d, void *InWindowHandle, uint32_t sizeX, uint32_t sizeY, PixelFormat InPreferredPixelFormat);
    ~VulkanViewport();
    std::shared_ptr<Texture> GetBackBuffer(/*RHICommandListImmediate &RHICmdList*/);

    virtual void WaitForFrameEventCompletion();

    virtual void IssueFrameEvent();

    inline IntVec2 GetSizeXY() const { return IntVec2(sizeX, sizeY); }

    bool Present(CommandListContext *context, CmdBuffer *cmdBuffer, Queue *queue, Queue *presentQueue, bool bLockToVsync);

    inline bool IsFullscreen() const { return false; }
    inline uint32 GetBackBufferImageCount() { return (uint32)backBufferImages.size(); }
    inline VkImage GetBackBufferImage(uint32 Index)
    {
        if (backBufferImages.size() > 0)
        {
            return backBufferImages[Index];
        }
        else
        {
            return VK_NULL_HANDLE;
        }
    }

    inline SwapChain *GetSwapChain() { return swapChain; }

    VkSurfaceTransformFlagBitsKHR GetSwapchainQCOMRenderPassTransform() const;
    VkFormat GetSwapchainImageFormat() const;

protected:
    std::vector<VkImage> backBufferImages;
    std::vector<VulkanRHI::Semaphore *> renderingDoneSemaphores;
    std::vector<VulkanView *> textureViews;
    std::shared_ptr<BackBuffer> RHIBackBuffer;
    // 'Dummy' back buffer
    std::shared_ptr<VulkanTexture> renderingBackBuffer;

    uint32_t sizeX;
    uint32_t sizeY;
    PixelFormat pixelFormat;
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
    void RecreateSwapchainFromRT(PixelFormat PreferredPixelFormat);
    void Resize(uint32 InSizeX, uint32 InSizeY, bool bIsFullscreen, PixelFormat PreferredPixelFormat);

    bool DoCheckedSwapChainJob(std::function<int32(VulkanViewport *)> SwapChainJob);
    bool SupportsStandardSwapchain();
    bool RequiresRenderingBackBuffer();

    PixelFormat GetPixelFormatForNonDefaultSwapchain();

    friend class BackBuffer;
    friend class RHI;
};