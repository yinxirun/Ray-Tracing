#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "GLFW/glfw3.h"
#include "gpu/RHI/dynamic_rhi.h"
#include "gpu/RHI/RHI.h"
#include "gpu/RHI/pipeline_state_cache.h"
#include "gpu/render_core/static_states.h"
#include "gpu/vk/context.h"
#include "gpu/vk/viewport.h"

Viewport *drawingViewport = nullptr;

void OnSizeChanged(GLFWwindow *window, int width, int height)
{
    rhi->ResizeViewport(drawingViewport, width, height, false, PF_Unknown);
    // printf("Resize Viewport. Width: %d Height: %d\n", width, height);
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

#define WIDTH 800
#define HEIGHT 600

std::vector<uint8> LoadShader(std::string filename, ShaderFrequency freq);

struct PerCamera
{
    Mat4 model;
    Mat4 view;
    Mat4 proj;
};

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Xi", nullptr, nullptr);
    glfwSetFramebufferSizeCallback(window, OnSizeChanged);

    RHIInit();
    RHICommandListImmediate dummy = RHICommandListExecutor::GetImmediateCommandList();
    {
        CommandContext *context = GetDefaultContext();
        std::shared_ptr<Viewport> viewport = CreateViewport(window, 800, 600, false, PixelFormat::PF_B8G8R8A8);
        drawingViewport = viewport.get();

        BufferDesc desc(36, 4, BufferUsageFlags::VertexBuffer);
        ResourceCreateInfo ci{};
        auto positionBuffer = CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);
        auto colorBuffer = CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);

        desc = BufferDesc(12, 4, BufferUsageFlags::IndexBuffer);
        auto indexBuffer = CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);

        void *mapped;

        mapped = LockBuffer_BottomOfPipe(dummy, indexBuffer.get(), 0, indexBuffer->GetSize(), ResourceLockMode::RLM_WriteOnly);
        uint32 indices[3] = {0, 1, 2};
        memcpy(mapped, indices, 12);
        UnlockBuffer_BottomOfPipe(dummy, indexBuffer.get());

        mapped = LockBuffer_BottomOfPipe(dummy, positionBuffer.get(), 0, positionBuffer->GetSize(), ResourceLockMode::RLM_WriteOnly);
        float position[9] = {0.5f, -0.5f, 0.5f, 0.f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f};
        memcpy(mapped, position, 36);
        UnlockBuffer_BottomOfPipe(dummy, positionBuffer.get());

        mapped = LockBuffer_BottomOfPipe(dummy, colorBuffer.get(), 0, colorBuffer->GetSize(), ResourceLockMode::RLM_WriteOnly);
        float color[9] = {1.0f, 0.0f, 0.0f, 0.f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
        memcpy(mapped, color, 36);
        UnlockBuffer_BottomOfPipe(dummy, colorBuffer.get());

        // 创建PSO
        GraphicsPipelineStateInitializer graphicsPSOInit;

        graphicsPSOInit.RenderTargetsEnabled = 1;
        graphicsPSOInit.RenderTargetFormats[0] = PF_B8G8R8A8;
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
        elements.push_back(VertexElement(0, 0, VET_Float3, 0, 12));
        elements.push_back(VertexElement(1, 0, VET_Float3, 1, 12));
        graphicsPSOInit.BoundShaderState.VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(elements);
        graphicsPSOInit.BoundShaderState.VertexShaderRHI = CreateVertexShader(LoadShader("gpu/shaders/a.vert.spv", SF_Vertex));
        graphicsPSOInit.BoundShaderState.PixelShaderRHI = CreatePixelShader(LoadShader("gpu/shaders/a.frag.spv", SF_Pixel));
        // PSO的回收还没写，目前是会泄漏的
        auto *pso = CreateGraphicsPipelineState(graphicsPSOInit);

        PerCamera perCamera;
        perCamera.model = Rotate(Mat4(1), Radians(90), Vec3(0, 0, 1));
        perCamera.view = Mat4(1);
        perCamera.proj = Mat4(1);
        UniformBufferLayoutInitializer UBInit;
        UBInit.BindingFlags = UniformBufferBindingFlags::Shader;
        UBInit.ConstantBufferSize = sizeof(PerCamera);
        UBInit.Resources.push_back({offsetof(PerCamera, model), UniformBufferBaseType::UBMT_FLOAT32});
        UBInit.Resources.push_back({offsetof(PerCamera, view), UniformBufferBaseType::UBMT_FLOAT32});
        UBInit.Resources.push_back({offsetof(PerCamera, proj), UniformBufferBaseType::UBMT_FLOAT32});
        UBInit.ComputeHash();
        auto UBLayout = std::make_shared<const UniformBufferLayout>(UBInit);
        auto ub = CreateUniformBuffer(0, UBLayout, UniformBufferUsage::UniformBuffer_MultiFrame, UniformBufferValidation::None);
        rhi->UpdateUniformBuffer(dummy, ub.get(), &perCamera);
        context->SubmitCommandsHint();

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            context->BeginDrawingViewport(viewport);
            context->BeginFrame();

            RenderPassInfo RPInfo(viewport->GetBackBuffer().get(), RenderTargetActions::Clear_Store);
            context->BeginRenderPass(RPInfo, "no name");

            context->SetGraphicsPipelineState(pso, 0, false);
            context->SetShaderUniformBuffer(graphicsPSOInit.BoundShaderState.VertexShaderRHI, 0, ub.get());

            context->SetStreamSource(0, positionBuffer.get(), 0);
            context->SetStreamSource(1, colorBuffer.get(), 0);
            context->DrawIndexedPrimitive(indexBuffer.get(), 0, 0, 3, 0, 1, 1);

            context->EndRenderPass();

            context->EndFrame();
            context->EndDrawingViewport(viewport.get(), false);
        }
        context->SubmitCommandsHint();
    }

    RHIExit();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}