#define VK_NO_PROTOTYPES
#include "window.h"
#include "vulkan_base.h"
#include "context.h"

static void framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    auto app = reinterpret_cast<Context *>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

void Window::InitWindow(Context *onwer)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Xi", nullptr, nullptr);

    glfwSetWindowUserPointer(window, onwer);

    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void Window::cleanup()
{
    glfwDestroyWindow(window);

    glfwTerminate();
}

VkResult Window::CreateWindowSurface(VkInstance instance, VkSurfaceKHR *surface)
{
    return glfwCreateWindowSurface(instance, window, nullptr, surface);
}

const char **Window::GetRequiredInstanceExtensions(uint32_t *count)
{
    return glfwGetRequiredInstanceExtensions(count);
}