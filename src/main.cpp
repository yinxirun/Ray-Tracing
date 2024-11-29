#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>

#include "GLFW/glfw3.h"
#include "engine/classes/static_mesh.h"
#include "RHI/dynamic_rhi.h"
#include "RHI/RHI.h"
#include "RHI/pipeline_state_cache.h"
#include "render_core/static_states.h"
#include "vk/context.h"
#include "vk/viewport.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

extern void load_wavefront_static_mesh(std::string inputfile, StaticMesh &staticMesh);
extern std::vector<uint8> process_shader(std::string filename, ShaderFrequency freq);

VulkanViewport *drawingViewport = nullptr;
extern RHICommandListExecutor GRHICommandListExecutor;

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

#define WIDTH 1600
#define HEIGHT 1200

struct PerCamera
{
    Mat4 model;
    Mat4 view;
    Mat4 proj;
};

int RHITest()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // 固定窗口大小
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Xi", nullptr, nullptr);
    glfwSetFramebufferSizeCallback(window, OnSizeChanged);

    RHIInit();
    RHICommandListImmediate &dummy = RHICommandListExecutor::GetImmediateCommandList();
    dummy.SwitchPipeline(RHIPipeline::Graphics);
    {
        CommandContext *context = GetDefaultContext();
        std::shared_ptr<VulkanViewport> viewport = CreateViewport(window, WIDTH, HEIGHT, false, PixelFormat::PF_B8G8R8A8);
        drawingViewport = viewport.get();

        std::array<uint32, 12> indices = {4, 5, 6, 4, 6, 7,
                                          0, 1, 2, 0, 2, 3};
        std::array<float, 24> position = {0.5, -0.5, 0.5, 0.5, 0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5,
                                          0.5, -0.5, 0.6, 0.5, 0.5, 0.6, -0.5, 0.5, 0.6, -0.5, -0.5, 0.6};
        std::array<float, 24> color = {1.0f, 0.0f, 0.0f, 0.f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1, 1, 1,
                                       1.0f, 0.0f, 0.0f, 1.f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1, 0, 0};
        std::array<float, 16> uv = {0, -0, 0, 1, 1, 0, 1, 1,
                                    0, -0, 0, 1, 1, 0, 1, 1};

        BufferDesc desc(position.size() * sizeof(float), sizeof(float), BufferUsageFlags::VertexBuffer);
        ResourceCreateInfo ci{};
        auto positionBuffer = CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);
        auto colorBuffer = CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);

        desc = BufferDesc(uv.size() * sizeof(float), sizeof(float), BufferUsageFlags::VertexBuffer);
        auto texCoordBuffer = CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);

        desc = BufferDesc(indices.size() * sizeof(uint32), sizeof(uint32), BufferUsageFlags::IndexBuffer);
        auto indexBuffer = CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);

        void *mapped;

        mapped = LockBuffer_BottomOfPipe(dummy, indexBuffer.get(), 0, indexBuffer->GetSize(), ResourceLockMode::RLM_WriteOnly);
        memcpy(mapped, indices.data(), indices.size() * sizeof(uint32));
        UnlockBuffer_BottomOfPipe(dummy, indexBuffer.get());

        mapped = LockBuffer_BottomOfPipe(dummy, positionBuffer.get(), 0, positionBuffer->GetSize(), ResourceLockMode::RLM_WriteOnly);
        memcpy(mapped, position.data(), position.size() * sizeof(float));
        UnlockBuffer_BottomOfPipe(dummy, positionBuffer.get());

        mapped = LockBuffer_BottomOfPipe(dummy, colorBuffer.get(), 0, colorBuffer->GetSize(), ResourceLockMode::RLM_WriteOnly);
        memcpy(mapped, color.data(), color.size() * sizeof(float));
        UnlockBuffer_BottomOfPipe(dummy, colorBuffer.get());

        mapped = LockBuffer_BottomOfPipe(dummy, texCoordBuffer.get(), 0, texCoordBuffer->GetSize(), ResourceLockMode::RLM_WriteOnly);
        memcpy(mapped, uv.data(), uv.size() * sizeof(float));
        UnlockBuffer_BottomOfPipe(dummy, texCoordBuffer.get());

        // 创建PSO
        GraphicsPipelineStateInitializer graphicsPSOInit;

        graphicsPSOInit.RenderTargetsEnabled = 1;
        graphicsPSOInit.RenderTargetFormats[0] = PF_B8G8R8A8;
        graphicsPSOInit.RenderTargetFlags[0] = TextureCreateFlags::RenderTargetable;
        graphicsPSOInit.NumSamples = 1;

        graphicsPSOInit.DepthStencilTargetFormat = PF_D24;

        graphicsPSOInit.SubpassHint = SubpassHint::None;
        graphicsPSOInit.SubpassIndex = 0;

        graphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI().get();
        graphicsPSOInit.DepthStencilState = TStaticDepthStencilState<>::GetRHI().get();

        graphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI().get();
        graphicsPSOInit.PrimitiveType = PT_TriangleList;

        VertexDeclarationElementList elements;
        elements.push_back(VertexElement(0, 0, VET_Float3, 0, 12));
        elements.push_back(VertexElement(1, 0, VET_Float3, 1, 12));
        elements.push_back(VertexElement(2, 0, VET_Float2, 2, 8));
        graphicsPSOInit.BoundShaderState.VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(elements);
        graphicsPSOInit.BoundShaderState.VertexShaderRHI = CreateVertexShader(process_shader("shaders/a.vert.spv", SF_Vertex));
        graphicsPSOInit.BoundShaderState.PixelShaderRHI = CreatePixelShader(process_shader("shaders/a.frag.spv", SF_Pixel));

        // 相机参数
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

        // 纹理
        int x, y, comp;
        stbi_uc *sourceData = stbi_load("assets/viking_room.png", &x, &y, &comp, STBI_rgb_alpha);
        for (int i = 0; i < x * y; ++i)
        {
            std::swap(sourceData[i * STBI_rgb_alpha + 0], sourceData[i * STBI_rgb_alpha + 2]);
        }
        TextureCreateDesc texDesc = TextureCreateDesc::Create2D("BaseColor", x, y, PF_B8G8R8A8)
                                        .SetInitialState(Access::SRVGraphics)
                                        .SetFlags(TexCreate_SRGB | TexCreate_ShaderResource);
        auto tex = CreateTexture(dummy, texDesc);
        UpdateTextureRegion2D region(0, 0, 0, 0, x, y);
        UpdateTexture2D(dummy, tex.get(), 0, region, x * STBI_rgb_alpha, sourceData);
        stbi_image_free(sourceData);
        SamplerStateInitializer samplerInit(SF_Point);
        auto sampler = CreateSamplerState(samplerInit);

        // 深度图
        texDesc.SetFormat(PF_D24)
            .SetInitialState(Access::DSVRead | Access::DSVWrite)
            .SetFlags(TexCreate_DepthStencilTargetable)
            .SetExtent(WIDTH, HEIGHT)
            .SetClearValue(ClearValueBinding(0, 0));
        auto depth = CreateTexture(dummy, texDesc);

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            context->BeginDrawingViewport(viewport);
            context->BeginFrame();

            RenderPassInfo RPInfo(viewport->GetBackBuffer().get(), RenderTargetActions::Clear_Store,
                                  depth.get(), EDepthStencilTargetActions::ClearDepthStencil_DontStoreDepthStencil);
            context->BeginRenderPass(RPInfo, "no name");

            // PSO的回收还没写，目前是会泄漏的
            auto *pso = CreateGraphicsPipelineState(graphicsPSOInit);
            context->SetGraphicsPipelineState(pso, 0, false);
            context->SetShaderUniformBuffer(graphicsPSOInit.BoundShaderState.VertexShaderRHI, 0, ub.get());
            context->SetShaderTexture(graphicsPSOInit.BoundShaderState.PixelShaderRHI, 0, tex.get());
            context->SetShaderSampler(graphicsPSOInit.BoundShaderState.PixelShaderRHI, 0, sampler.get());

            context->SetStreamSource(0, positionBuffer.get(), 0);
            context->SetStreamSource(1, colorBuffer.get(), 0);
            context->SetStreamSource(2, texCoordBuffer.get(), 0);
            context->DrawIndexedPrimitive(indexBuffer.get(), 0, 0, 8, 0, 4, 1);

            context->EndRenderPass();

            context->EndFrame();
            context->EndDrawingViewport(viewport.get(), false);
        }
        context->SubmitCommandsHint();
    }

    /*validation layer: Validation Error: [ VUID-vkDestroyBuffer-buffer-00922 ] | MessageID = 0xe4549c11 | Cannot call vkDestroyBuffer on VkBuffer 0xba7514000000002a[] that is currently in use by a command buffer. The Vulkan spec states: All submitted commands that refer to buffer, either directly or via a VkBufferView, must have completed execution (https://vulkan.lunarg.com/doc/view/1.3.261.1/windows/1.3-extensions/vkspec.html#VUID-vkDestroyBuffer-buffer-00922)
    这个报错，index buffer color buffer uv buffer uniform buffer还绑定在cmd上却被要求销毁
    */
    RHIExit();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

#include "engine/classes/components/static_mesh_component.h"
#include "engine/scene_view.h"
#include "renderer/scene_private.h"
#include "renderer/renderer_module.h"
#include "renderer/scene_rendering.h"
RendererModule module;
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // 固定窗口大小
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Xi", nullptr, nullptr);
    glfwSetFramebufferSizeCallback(window, OnSizeChanged);

    RHIInit();
    RHICommandListImmediate &dummy = RHICommandListExecutor::GetImmediateCommandList();
    dummy.SwitchPipeline(RHIPipeline::Graphics);
    {
        // 加载模型
        auto staticMesh = std::make_shared<StaticMesh>();
        load_wavefront_static_mesh("assets/cornell-box/cornell-box.obj", *staticMesh);
        auto component = std::make_shared<StaticMeshComponent>();
        component->SetStaticMesh(staticMesh);

        Scene scene;
        scene.AddPrimitive(std::static_pointer_cast<PrimitiveComponent>(component));
        std::vector<PrimitiveComponent *> primitivesView;
        for (auto &primitive : scene.primitives)
        {
            primitivesView.push_back(primitive.get());
        }
        PrimitiveComponent::AddStaticMeshes(dummy, &scene, primitivesView, false);

        CommandContext *context = GetDefaultContext();
        std::shared_ptr<VulkanViewport> viewport = CreateViewport(window, WIDTH, HEIGHT, false, PixelFormat::PF_B8G8R8A8);
        drawingViewport = viewport.get();

        // 设置相机。一个ViewFamily代表一个相机，一个相机有多个视口（View）
        SceneView view;
        view.FOV = 39.3077;
        view.NearClippingDistance = 800;
        view.farClippingDistance = 2000;
        view.ViewLocation = Vec3(278, 273, -800);
        view.ViewRect = IntVec2(WIDTH, HEIGHT);
        view.ViewRotation = Mat4(1);
        view.WorldToMetersScale = 1;

        CameraInfo viewFamily(Camera{});
        viewFamily.renderTarget = drawingViewport->GetBackBuffer().get();
        viewFamily.views.push_back(&view);
        viewFamily.scene = &scene;
        TextureCreateDesc texDesc =
            TextureCreateDesc::Create2D("Base Color", view.ViewRect.x, view.ViewRect.y, PF_B8G8R8A8)
                .SetInitialState(Access::RTV | Access::SRVGraphics)
                .SetFlags(TexCreate_RenderTargetable);
        viewFamily.GetSceneTextures().GBufferA = CreateTexture(dummy, texDesc);
        viewFamily.GetSceneTextures().GBufferB = CreateTexture(dummy, texDesc.SetFormat(PF_B8G8R8A8).SetDebugName("Normal"));
        viewFamily.GetSceneTextures().GBufferC = CreateTexture(dummy, texDesc.SetFormat(PF_FloatRGB).SetDebugName("Position"));
        viewFamily.GetSceneTextures().GBufferD = CreateTexture(dummy, texDesc.SetFormat(PF_B8G8R8A8).SetDebugName("Roughness-Metallic"));

        // 运行
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            context->BeginDrawingViewport(viewport);
            context->BeginFrame();

            module.BeginRenderingViewFamily(&viewFamily);

            context->EndFrame();
            context->EndDrawingViewport(viewport.get(), false);
        }
    }
    RHIExit();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}