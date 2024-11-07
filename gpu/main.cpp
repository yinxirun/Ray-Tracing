#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

#include "gpu/application.h"
#include "gpu/vk/viewport.h"
#include "gpu/vk/context.h"
#include "gpu/vk/rhi.h"
#include "gpu/RHI/RHI.h"
#include "gpu/RHI/pipeline_state_cache.h"

#include "gpu/render_core/static_states.h"

Viewport *drawingViewport = nullptr;
RHICommandList dummy;

void OnSizeChanged(GLFWwindow *window, int width, int height)
{
    rhi->ResizeViewport(drawingViewport, width, height, false, PF_Unknown);
    printf("Resize Viewport. Width: %d Height: %d\n", width, height);
}

std::vector<uint8> readFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<uint8> buffer(fileSize);
    file.seekg(0);
    file.read((char *)buffer.data(), fileSize);
    file.close();
    return buffer;
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Xi", nullptr, nullptr);
    glfwSetFramebufferSizeCallback(window, OnSizeChanged);

    rhi = &RHI::Get();
    rhi->Init();
    {
        CommandListContext *context = rhi->GetDefaultContext();
        std::shared_ptr<Viewport> viewport = rhi->CreateViewport(window, 800, 600, false, PixelFormat::PF_B8G8R8A8);
        drawingViewport = viewport.get();

        BufferDesc desc(36, 0, BufferUsageFlags::VertexBuffer);
        ResourceCreateInfo ci{};
        auto positionBuffer = rhi->CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);
        auto colorBuffer = rhi->CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);

        void *mapped = rhi->LockBuffer_BottomOfPipe(dummy, positionBuffer.get(), 0, positionBuffer->GetSize(), ResourceLockMode::RLM_WriteOnly);
        float position[9] = {0.5f, -0.5f, 0.f, 0.f, 0.5f, 0.f, -0.5f, -0.5f, 0.f};
        memcpy(mapped, position, 36);
        rhi->UnlockBuffer_BottomOfPipe(dummy, positionBuffer.get());

        mapped = rhi->LockBuffer_BottomOfPipe(dummy, colorBuffer.get(), 0, colorBuffer->GetSize(), ResourceLockMode::RLM_WriteOnly);
        float color[9] = {0, 0, 1, 0, 1, 0, 1, 0, 0};
        memcpy(mapped, color, 36);
        rhi->UnlockBuffer_BottomOfPipe(dummy, colorBuffer.get());

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            context->BeginDrawingViewport(viewport);
            context->BeginFrame();

            // 创建PSO
            GraphicsPipelineStateInitializer graphicsPSOInit;

            graphicsPSOInit.RenderTargetsEnabled = 1;
            graphicsPSOInit.RenderTargetFormats[0] = PF_Unknown;
            graphicsPSOInit.RenderTargetFlags[0] = TextureCreateFlags::RenderTargetable;
            graphicsPSOInit.NumSamples = 1;
            graphicsPSOInit.DepthStencilTargetFormat = PF_Unknown;

            graphicsPSOInit.SubpassHint = SubpassHint::None;
            graphicsPSOInit.SubpassIndex = 0;

            graphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI().get();
            graphicsPSOInit.DepthStencilState = TStaticDepthStencilState<>::GetRHI().get();

            graphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI().get();
            graphicsPSOInit.PrimitiveType = PT_TriangleList;

            VertexDeclarationElementList elements;
            elements.push_back(VertexElement(0, 0, VET_Float4, 0, 12));
            graphicsPSOInit.BoundShaderState.VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(elements);
            graphicsPSOInit.BoundShaderState.VertexShaderRHI = 0;
            graphicsPSOInit.BoundShaderState.PixelShaderRHI = 0;

            RenderPassInfo renderpassInfo{};
            RenderPassInfo::ColorEntry entry;
            entry.ArraySlice = 0;
            entry.RenderTarget = viewport->GetBackBuffer().get();
            entry.Action = RenderTargetActions::Clear_Store;
            renderpassInfo.ColorRenderTargets[0] = entry;
            context->BeginRenderPass(renderpassInfo, "test");

            context->SetStreamSource(0, positionBuffer.get(), 0);

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