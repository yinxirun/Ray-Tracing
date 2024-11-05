#pragma once
#include <Volk/volk.h>
#include <GLFW/glfw3.h>

const int WIDTH = 800;
const int HEIGHT = 600;

class VulkanBase;
class Context;

class Window
{
public:
    GLFWwindow *window;

    void InitWindow(Context *context);

    void cleanup();

    GLFWwindow *GetHandle() const
    {
        return window;
    }

    int WindowShouldClose()
    {
        return glfwWindowShouldClose(window);
    }

    void PollEvents()
    {
        glfwPollEvents();
    }

    void WaitEvents()
    {
        glfwWaitEvents();
    }

    void GetFramebufferSize(int *width, int *height) const
    {
        glfwGetFramebufferSize(window, width, height);
    }

    VkResult CreateWindowSurface(VkInstance instance, VkSurfaceKHR *surface);

    const char **GetRequiredInstanceExtensions(uint32_t *count);
};