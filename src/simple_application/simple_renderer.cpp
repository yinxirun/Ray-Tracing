#include "simple_renderer.h"
#include "simple_scene.h"

#include "RHI/RHIResources.h"
#include "RHI/dynamic_rhi.h"
#include "RHI/RHIContext.h"
#include "RHI/RHICommandList.h"
#include "RHI/pipeline_state_cache.h"
#include "render_core/static_states.h"

extern std::vector<uint8> process_shader(std::string spvFilename, ShaderFrequency freq);
const int WIDTH = 1600;
const int HEIGHT = 1200;

SimpleRenderer::SimpleRenderer(SimpleScene &s) : scene(s) {}

void SimpleRenderer::Prepare(RHICommandListBase &immediate)
{
    TextureCreateDesc texDesc;
    texDesc.SetFormat(PF_D24)
        .SetInitialState(Access::DSVRead | Access::DSVWrite)
        .SetFlags(TexCreate_DepthStencilTargetable)
        .SetExtent(WIDTH, HEIGHT)
        .SetClearValue(ClearValueBinding(0, 0));
    depth = CreateTexture(immediate, texDesc);

    // 相机参数
    CameraInfo perCamera;
    perCamera.model = Rotate(Mat4(1), Radians(0), Vec3(0, 0, 1));
    perCamera.view = Lookat(Vec3(0, 0, -1), Vec3(0, 0, 0), Vec3(0, 1, 0));
    perCamera.proj = Perspective(Radians(60), (float)WIDTH / (float)HEIGHT, 100, 0.1);
    UniformBufferLayoutInitializer UBInit;
    UBInit.BindingFlags = UniformBufferBindingFlags::Shader;
    UBInit.ConstantBufferSize = sizeof(CameraInfo);
    UBInit.Resources.push_back({offsetof(CameraInfo, model), UniformBufferBaseType::UBMT_FLOAT32});
    UBInit.Resources.push_back({offsetof(CameraInfo, view), UniformBufferBaseType::UBMT_FLOAT32});
    UBInit.Resources.push_back({offsetof(CameraInfo, proj), UniformBufferBaseType::UBMT_FLOAT32});
    UBInit.ComputeHash();
    auto UBLayout = std::make_shared<const UniformBufferLayout>(UBInit);
    ub = CreateUniformBuffer(0, UBLayout, UniformBufferUsage::UniformBuffer_MultiFrame, UniformBufferValidation::None);
    rhi->UpdateUniformBuffer(immediate, ub.get(), &perCamera);
    immediate.GetContext().SubmitCommandsHint();
}

void SimpleRenderer::Render(CommandContext *context, SimpleScene &scene, Viewport *viewport)
{
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
    graphicsPSOInit.BoundShaderState.VertexShaderRHI = CreateVertexShader(process_shader("shaders/test_cube.vert.spv", SF_Vertex));
    graphicsPSOInit.BoundShaderState.PixelShaderRHI = CreatePixelShader(process_shader("shaders/test_cube.frag.spv", SF_Pixel));

    RenderPassInfo RPInfo(GetViewportBackBuffer(viewport).get(), RenderTargetActions::Clear_Store,
                          depth.get(), EDepthStencilTargetActions::ClearDepthStencil_DontStoreDepthStencil);
    context->BeginRenderPass(RPInfo, "no name");
    for (SimpleStaticMesh &mesh : scene.staticMeshes)
    {
        auto *pso = CreateGraphicsPipelineState(graphicsPSOInit);
        context->SetGraphicsPipelineState(pso, 0, false);
        context->SetShaderUniformBuffer(graphicsPSOInit.BoundShaderState.VertexShaderRHI, 0, ub.get());

        context->SetStreamSource(0, mesh.LOD[0].positonBuffer.get(), 0);
        context->SetStreamSource(1, mesh.LOD[0].normalBuffer.get(), 0);
        context->SetStreamSource(2, mesh.LOD[0].uvBuffer.get(), 0);
        context->DrawIndexedPrimitive(mesh.LOD[0].indexBuffer.get(), 0, 0, mesh.LOD[0].position.size(), 0, mesh.LOD[0].position.size() / 3, 1);
    }
    context->EndRenderPass();
}