#include "platform.h"
#include "GLFW/glfw3.h"

VkResult GenericPlatform::CreateSwapchainKHR(void *WindowHandle, VkPhysicalDevice PhysicalDevice, VkDevice Device, const VkSwapchainCreateInfoKHR *CreateInfo, const VkAllocationCallbacks *Allocator, VkSwapchainKHR *Swapchain)
{
    return vkCreateSwapchainKHR(Device, CreateInfo, Allocator, Swapchain);
}

void GenericPlatform::DestroySwapchainKHR(VkDevice Device, VkSwapchainKHR Swapchain, const VkAllocationCallbacks *Allocator)
{
    vkDestroySwapchainKHR(Device, Swapchain, Allocator);
}

// Allow platforms to do extra work on present
VkResult GenericPlatform::Present(VkQueue Queue, VkPresentInfoKHR &PresentInfo)
{
    return vkQueuePresentKHR(Queue, &PresentInfo);
}

void WindowsPlatform::CreateSurface(void *windowHandle, VkInstance instance, VkSurfaceKHR *outSurface)
{
    glfwCreateWindowSurface(instance, (GLFWwindow *)windowHandle, nullptr, outSurface);
}