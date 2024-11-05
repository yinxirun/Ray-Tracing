#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

#include "gpu/application.h"
#include "gpu/vk/viewport.h"
#include "gpu/vk/context.h"
#include "gpu/vk/rhi.h"

Viewport *drawingViewport = nullptr;
RHICommandList dummy;

void OnSizeChanged(GLFWwindow *window, int width, int height)
{
    rhi->ResizeViewport(drawingViewport, width, height, false, PF_Unknown);
    printf("Resize Viewport. Width: %d Height: %d\n", width, height);
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Xi", nullptr, nullptr);
    glfwSetFramebufferSizeCallback(window, OnSizeChanged);

    rhi = new RHI();
    rhi->Init();
    {
        CommandListContext *context = rhi->GetDefaultContext();
        std::shared_ptr<Viewport> viewport = rhi->CreateViewport(window, 800, 600, false, PixelFormat::PF_B8G8R8A8);
        drawingViewport = viewport.get();

        BufferDesc desc(24, 0, BufferUsageFlags::VertexBuffer);
        ResourceCreateInfo ci{};
        auto positionBuffer = rhi->CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);
        auto colorBuffer = rhi->CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);

        void *mapped = rhi->LockBuffer_BottomOfPipe(dummy, positionBuffer.get(), 0, positionBuffer->GetSize(), ResourceLockMode::RLM_WriteOnly);
        rhi->UnlockBuffer_BottomOfPipe(dummy, positionBuffer.get());

        auto storage = rhi->CreateBuffer(desc, Access::DSVWrite | Access::DSVRead, ci);

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            context->BeginDrawingViewport(viewport);
            context->BeginFrame();

            RenderPassInfo renderpassInfo{};
            RenderPassInfo::ColorEntry entry;
            entry.ArraySlice = 0;
            entry.RenderTarget = viewport->GetBackBuffer().get();
            entry.Action = RenderTargetActions::Clear_Store;
            renderpassInfo.ColorRenderTargets[0] = entry;
            context->BeginRenderPass(renderpassInfo, "test");

            context->EndRenderPass();
            context->EndFrame();
            context->EndDrawingViewport(viewport.get(), false);
        }
    }

    rhi->Shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}