#pragma once

#include "Volk/volk.h"

class GenericPlatform
{
public:
    // Allow platforms to track swapchain creation
    static VkResult CreateSwapchainKHR(void *WindowHandle, VkPhysicalDevice PhysicalDevice, VkDevice Device, const VkSwapchainCreateInfoKHR *CreateInfo, const VkAllocationCallbacks *Allocator, VkSwapchainKHR *Swapchain);

    // Allow platforms to track swapchain destruction
    static void DestroySwapchainKHR(VkDevice Device, VkSwapchainKHR Swapchain, const VkAllocationCallbacks *Allocator);

    // Ensure the last frame completed on the GPU
    static bool RequiresWaitingForFrameCompletionEvent() { return true; }

    // Allow platforms to do extra work on present
    static VkResult Present(VkQueue Queue, VkPresentInfoKHR &PresentInfo);

    // Whether to attempt recreate swapchain when present or acqurire operations fail
    static bool RecreateSwapchainOnFail() { return true; }

    // Does the platform require depth to be written on stencil clear
    static bool RequiresDepthWriteOnStencilClear() { return false; }

    static bool RegisterGPUWork() { return true; }
};

class WindowsPlatform : public GenericPlatform
{
public:
    static void CreateSurface(void *WindowHandle, VkInstance Instance, VkSurfaceKHR *OutSurface);
};

typedef WindowsPlatform Platform;
