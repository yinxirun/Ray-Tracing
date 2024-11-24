#include "swap_chain.h"
#include "device.h"
#include "platform.h"
#include <cassert>
#include <algorithm>
#include "queue.h"
#include "configuration.h"
#include "vulkan_memory.h"
#include "definitions.h"
#include "util.h"

int32 GVulkanKeepSwapChain = 1;

SwapChain::SwapChain(VkInstance inInstance, Device &inDevice, void *windowHandle, VkFormat inOutPixelFormat,
                     uint32_t width, uint32_t height, uint32_t *InOutDesiredNumBackBuffers,
                     std::vector<VkImage> &outImages, SwapChainRecreateInfo *recreateInfo)
    : swapChain(VK_NULL_HANDLE), device(inDevice), surface(VK_NULL_HANDLE), windowHandle(windowHandle),
      currentImageIndex(-1), semaphoreIndex(0), numAcquireCalls(0), instance(inInstance)
{
    if (recreateInfo != nullptr && recreateInfo->swapChain != VK_NULL_HANDLE)
    {
        assert(recreateInfo->surface != VK_NULL_HANDLE);
        surface = recreateInfo->surface;
        recreateInfo->surface = VK_NULL_HANDLE;
    }
    else
    {
        // let the platform create the surface
        Platform::CreateSurface(windowHandle, instance, &surface);
    }

    uint32_t numFormats;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.GetPhysicalHandle(), surface, &numFormats, nullptr);
    assert(numFormats > 0);

    std::vector<VkSurfaceFormatKHR> formats(numFormats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.GetPhysicalHandle(), surface, &numFormats, formats.data());

    VkColorSpaceKHR requestedColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkFormat requestedFormat = inOutPixelFormat;
    VkSurfaceFormatKHR curFormat = formats[0];
    for (const auto &availableFormat : formats)
    {
        if (availableFormat.format == requestedFormat &&
            availableFormat.colorSpace == requestedColorSpace)
        {
            curFormat = availableFormat;
            break;
        }
    }

    device.SetupPresentQueue(surface);

    // Fetch present mode
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    {
        uint32_t NumFoundPresentModes = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device.GetPhysicalHandle(), surface, &NumFoundPresentModes, nullptr);
        assert(NumFoundPresentModes > 0);

        std::vector<VkPresentModeKHR> FoundPresentModes;
        FoundPresentModes.resize(NumFoundPresentModes);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device.GetPhysicalHandle(), surface, &NumFoundPresentModes, FoundPresentModes.data());

        presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto &availablePresentMode : FoundPresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                presentMode = availablePresentMode;
                break;
            }
        }
    }

    // Check the surface properties and formats
    VkSurfaceCapabilitiesKHR SurfProperties;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.GetPhysicalHandle(), surface, &SurfProperties);

    uint32_t DesiredNumBuffers = SurfProperties.maxImageCount > 0 ? std::clamp(*InOutDesiredNumBackBuffers, SurfProperties.minImageCount, SurfProperties.maxImageCount) : *InOutDesiredNumBackBuffers;
    uint32_t sizeX = SurfProperties.currentExtent.width == 0xFFFFFFFF ? width : SurfProperties.currentExtent.width;
    uint32_t sizeY = SurfProperties.currentExtent.height == 0xFFFFFFFF ? height : SurfProperties.currentExtent.height;

    VkSwapchainCreateInfoKHR SwapChainInfo;
    ZeroVulkanStruct(SwapChainInfo, VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
    SwapChainInfo.surface = surface;
    SwapChainInfo.minImageCount = DesiredNumBuffers;
    SwapChainInfo.imageFormat = curFormat.format;
    SwapChainInfo.imageColorSpace = curFormat.colorSpace;
    SwapChainInfo.imageExtent.width = sizeX;
    SwapChainInfo.imageExtent.height = sizeY;
    SwapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    SwapChainInfo.preTransform = SurfProperties.currentTransform;
    SwapChainInfo.imageArrayLayers = 1;
    SwapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SwapChainInfo.presentMode = presentMode;
    SwapChainInfo.oldSwapchain = VK_NULL_HANDLE;
    if (recreateInfo != nullptr)
    {
        SwapChainInfo.oldSwapchain = recreateInfo->swapChain;
    }

    SwapChainInfo.clipped = VK_TRUE;
    SwapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    {
        // #todo-rco: Crappy workaround
        if (SwapChainInfo.imageExtent.width == 0)
        {
            SwapChainInfo.imageExtent.width = width;
        }
        if (SwapChainInfo.imageExtent.height == 0)
        {
            SwapChainInfo.imageExtent.height = height;
        }
    }

    VkBool32 bSupportsPresent;
    vkGetPhysicalDeviceSurfaceSupportKHR(device.GetPhysicalHandle(), device.GetPresentQueue()->GetFamilyIndex(), surface, &bSupportsPresent);
    assert(bSupportsPresent);

    VkResult Result = Platform::CreateSwapchainKHR(windowHandle, device.GetPhysicalHandle(), device.GetInstanceHandle(), &SwapChainInfo, VULKAN_CPU_ALLOCATOR, &swapChain);

    if (recreateInfo != nullptr)
    {
        if (recreateInfo->swapChain != VK_NULL_HANDLE)
        {
            Platform::DestroySwapchainKHR(device.GetInstanceHandle(), recreateInfo->swapChain, VULKAN_CPU_ALLOCATOR);
            recreateInfo->swapChain = VK_NULL_HANDLE;
        }
        if (recreateInfo->surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(instance, recreateInfo->surface, VULKAN_CPU_ALLOCATOR);
            recreateInfo->surface = VK_NULL_HANDLE;
        }
    }

    internalWidth = std::min(width, SwapChainInfo.imageExtent.width);
    internalHeight = std::min(height, SwapChainInfo.imageExtent.height);

    uint32_t numSwapChainImages;
    vkGetSwapchainImagesKHR(device.GetInstanceHandle(), swapChain, &numSwapChainImages, nullptr);

    outImages.resize(numSwapChainImages);
    vkGetSwapchainImagesKHR(device.GetInstanceHandle(), swapChain, &numSwapChainImages, outImages.data());

#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
    imageAcquiredFences.resize(numSwapChainImages);
    VulkanRHI::FenceManager &fenceMgr = device.GetFenceManager();
    for (uint32 bufferIndex = 0; bufferIndex < numSwapChainImages; ++bufferIndex)
    {
        imageAcquiredFences[bufferIndex] = device.GetFenceManager().AllocateFence(true);
    }
#endif

    imageAcquiredSemaphore.resize(numSwapChainImages);
    for (uint32_t bufferIndex = 0; bufferIndex < numSwapChainImages; ++bufferIndex)
    {
        imageAcquiredSemaphore[bufferIndex] = new VulkanRHI::Semaphore(device);
        imageAcquiredSemaphore[bufferIndex]->AddRef();
    }

    *InOutDesiredNumBackBuffers = numSwapChainImages;

    presentID = 0;
}

void SwapChain::Destroy(SwapChainRecreateInfo *RecreateInfo)
{
    // We could be responding to an OUT_OF_DATE event and the GPU might not be done with swapchain image, so wait for idle.
    // Alternatively could also check on the fence(s) for the image(s) from the swapchain but then timing out/waiting could become an issue.
    device.WaitUntilIdle();

    bool bRecreate = RecreateInfo && GVulkanKeepSwapChain;
    if (bRecreate)
    {
        RecreateInfo->swapChain = swapChain;
        RecreateInfo->surface = surface;
    }
    else
    {
        Platform::DestroySwapchainKHR(device.GetInstanceHandle(), swapChain, VULKAN_CPU_ALLOCATOR);
    }
    swapChain = VK_NULL_HANDLE;

#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
    VulkanRHI::FenceManager& FenceMgr = device.GetFenceManager();
    for (int32 Index = 0; Index < imageAcquiredFences.size(); ++Index)
    {
    	FenceMgr.ReleaseFence(imageAcquiredFences[Index]);
    }
#endif

    // #todo-rco: Enqueue for deletion as we first need to destroy the cmd buffers and queues otherwise validation fails
    for (int BufferIndex = 0; BufferIndex < imageAcquiredSemaphore.size(); ++BufferIndex)
    {
        imageAcquiredSemaphore[BufferIndex]->Release();
    }

    if (!bRecreate)
    {
        vkDestroySurfaceKHR(instance, surface, VULKAN_CPU_ALLOCATOR);
    }

    // if (QCOMDepthView && QCOMDepthView != QCOMDepthStencilView)
    // {
    // 	delete QCOMDepthView;
    // 	QCOMDepthView = nullptr;
    // }

    // if (QCOMDepthStencilView)
    // {
    // 	delete QCOMDepthStencilView;
    // 	QCOMDepthStencilView = nullptr;
    // 	QCOMDepthView = nullptr;
    // }

    surface = VK_NULL_HANDLE;
}

SwapChain::EStatus SwapChain::Present(Queue *GfxQueue, Queue *PresentQueue, VulkanRHI::Semaphore *BackBufferRenderingDoneSemaphore)
{
    check(currentImageIndex != -1);

    VkPresentInfoKHR Info;
    ZeroVulkanStruct(Info, VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
    VkSemaphore Semaphore = VK_NULL_HANDLE;
    if (BackBufferRenderingDoneSemaphore)
    {
        Info.waitSemaphoreCount = 1;
        Semaphore = BackBufferRenderingDoneSemaphore->GetHandle();
        Info.pWaitSemaphores = &Semaphore;
    }
    Info.swapchainCount = 1;
    Info.pSwapchains = &swapChain;
    Info.pImageIndices = (uint32 *)&currentImageIndex;

    VkResult PresentResult;

    PresentResult = Platform::Present(PresentQueue->GetHandle(), Info);

    currentImageIndex = -1;

    if (PresentResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return EStatus::OutOfDate;
    }

    if (PresentResult == VK_ERROR_SURFACE_LOST_KHR)
    {
        return EStatus::SurfaceLost;
    }

    if (PresentResult != VK_SUCCESS && PresentResult != VK_SUBOPTIMAL_KHR)
    {
        VERIFYVULKANRESULT(PresentResult);
    }

    return EStatus::Healthy;
}

int32 SwapChain::AcquireImageIndex(VulkanRHI::Semaphore **OutSemaphore)
{
    check(currentImageIndex == -1);

    // Get the index of the next swapchain image we should render to.
    // We'll wait with an "infinite" timeout, the function will block until an image is ready.
    // The ImageAcquiredSemaphore[ImageAcquiredSemaphoreIndex] will get signaled when the image is ready (upon function return).
    uint32 imageIndex = 0;
    const int32 PrevSemaphoreIndex = semaphoreIndex;
    semaphoreIndex = (semaphoreIndex + 1) % imageAcquiredSemaphore.size();

    // If we have not called present for any of the swapchain images, it will cause a crash/hang
    // checkf(!(NumAcquireCalls == ImageAcquiredSemaphore.Num() - 1 && NumPresentCalls == 0), TEXT("vkAcquireNextImageKHR will fail as no images have been presented before acquiring all of them"));
#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
    VulkanRHI::FenceManager &fenceMgr = device.GetFenceManager();
    fenceMgr.ResetFence(imageAcquiredFences[semaphoreIndex]);
    const VkFence acquiredFence = imageAcquiredFences[semaphoreIndex]->GetHandle();
#else
    const VkFence AcquiredFence = VK_NULL_HANDLE;
#endif
    VkResult result;
    {
        const uint32 maxImageIndex = imageAcquiredSemaphore.size() - 1;

        // SCOPE_CYCLE_COUNTER(STAT_VulkanAcquireBackBuffer);
        // FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUPresent);

        result = vkAcquireNextImageKHR(
            device.GetInstanceHandle(),
            swapChain,
            UINT64_MAX,
            imageAcquiredSemaphore[semaphoreIndex]->GetHandle(),
            acquiredFence,
            &imageIndex);

        // The swapchain may have more images than we have requested on creating it. Ignore all extra images
        while (imageIndex > maxImageIndex && (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR))
        {
            printf("Fuck!! %s %d\n", __FILE__, __LINE__);
            exit(-1);
            result = vkAcquireNextImageKHR(
                device.GetInstanceHandle(),
                swapChain,
                UINT64_MAX,
                imageAcquiredSemaphore[semaphoreIndex]->GetHandle(),
                acquiredFence,
                &imageIndex);
        }
    }

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        semaphoreIndex = PrevSemaphoreIndex;
        return (int32)EStatus::OutOfDate;
    }

    if (result == VK_ERROR_SURFACE_LOST_KHR)
    {
        semaphoreIndex = PrevSemaphoreIndex;
        return (int32)EStatus::SurfaceLost;
    }

    ++numAcquireCalls;
    *OutSemaphore = imageAcquiredSemaphore[semaphoreIndex];

#if VULKAN_HAS_DEBUGGING_ENABLED
    if (Result == VK_ERROR_VALIDATION_FAILED_EXT)
    {
        extern TAutoConsoleVariable<int32> GValidationCvar;
        if (GValidationCvar.GetValueOnRenderThread() == 0)
        {
            UE_LOG(LogVulkanRHI, Fatal, TEXT("vkAcquireNextImageKHR failed with Validation error. Try running with r.Vulkan.EnableValidation=1 to get information from the driver"));
        }
    }
    else
#endif
    {
        // checkf(Result == VK_SUCCESS || Result == VK_SUBOPTIMAL_KHR, TEXT("vkAcquireNextImageKHR failed Result = %d"), int32(Result));
    }
    currentImageIndex = (int32)imageIndex;

#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
    {
        // SCOPE_CYCLE_COUNTER(STAT_VulkanWaitSwapchain);
        bool bResult = fenceMgr.WaitForFence(imageAcquiredFences[semaphoreIndex], UINT64_MAX);
        ensure(bResult);
    }
#endif
    return currentImageIndex;
}