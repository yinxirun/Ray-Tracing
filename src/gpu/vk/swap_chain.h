#pragma once

#include "Volk/volk.h"
#include "configuration.h"
#include "gpu/definitions.h"
#include <vector>

namespace VulkanRHI
{
    class Semaphore;
    class Fence;
}

class Device;
class Queue;

struct SwapChainRecreateInfo
{
    VkSwapchainKHR swapChain;
    VkSurfaceKHR surface;
};

class SwapChain
{
public:
    SwapChain(VkInstance inInstance, Device &inDevice, void *windowHandle, VkFormat inOutPixelFormat,
              uint32_t width, uint32_t height, uint32_t *InOutDesiredNumBackBuffers, std::vector<VkImage> &outImages, SwapChainRecreateInfo *recreatefInfo);

    void Destroy(SwapChainRecreateInfo *RecreateInfo);

    // Has to be negative as we use this also on other callbacks as the acquired image index
    enum class EStatus
    {
        Healthy = 0,
        OutOfDate = -1,
        SurfaceLost = -2,
    };

    EStatus Present(Queue *GfxQueue, Queue *PresentQueue, VulkanRHI::Semaphore *BackBufferRenderingDoneSemaphore);

protected:
    VkSurfaceTransformFlagBitsKHR QCOMRenderPassTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;

    VkSwapchainKHR swapChain;
    Device &device;

    VkSurfaceKHR surface;
    void *windowHandle;

    int32_t currentImageIndex;
    int32_t semaphoreIndex;
    uint32 numAcquireCalls;
    uint32 internalWidth = 0;
    uint32 internalHeight = 0;

    VkInstance instance;
    // 若该image显示完毕，则该信号激活，表示此时image可以被写了。
    std::vector<VulkanRHI::Semaphore *> imageAcquiredSemaphore;
#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
    // 若GPU端提交完毕，此处fence被激活
    std::vector<VulkanRHI::Fence *> imageAcquiredFences;
#endif
    uint32_t presentID = 0;

    int32 AcquireImageIndex(VulkanRHI::Semaphore **OutSemaphore);

    friend class Viewport;
    friend class Queue;
};