#include "engine/scene_interface.h"
#include "engine/scene_view.h"
#include "render_core/static_states.h"
#include "RHI/RHICommandList.h"
#include "RHI/RHIContext.h"
#include "RHI/dynamic_rhi.h"
#include "scene_rendering.h"
#include "renderer_module.h"
#include "scene_private.h"
#include <memory>
#include <array>

ViewFamilyInfo::ViewFamilyInfo(const SceneViewFamily &InViewFamily) : SceneViewFamily(InViewFamily)
{
    bIsViewFamilyInfo = true;
}

SceneRenderer::SceneRenderer(const SceneViewFamily *InViewFamily) : ViewFamily(*InViewFamily)
{
}

void SceneRenderer::CreateSceneRenderers(std::vector<const SceneViewFamily *> InViewFamilies, std::vector<SceneRenderer *> &out)
{
    const SceneInterface *scene = InViewFamilies[0]->scene;
    check(scene);
    for (int32 FamilyIndex = 0; FamilyIndex < InViewFamilies.size(); FamilyIndex++)
    {
        const SceneViewFamily *InViewFamily = InViewFamilies[FamilyIndex];
        check(InViewFamily);
        check(InViewFamily->scene == scene);
        auto renderer = new SceneRenderer(InViewFamily);
        out.push_back(renderer);
    }
}

void SceneRenderer::InitViews()
{
}

void SceneRenderer::SetupMeshPass(SceneView &View, ExclusiveDepthStencil::Type BasePassDepthStencilAccess, ViewCommands &ViewCommands)
{
    for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; PassIndex++)
    {
        const EMeshPass::Type PassType = (EMeshPass::Type)PassIndex;
    }
}

void SceneRenderer::Render()
{
    GraphicsPipelineStateInitializer graphicsPSOInit;
    graphicsPSOInit.RenderTargetsEnabled = 1;
    graphicsPSOInit.RenderTargetFormats[0] = ViewFamily.renderTarget->GetDesc().Format;
    graphicsPSOInit.RenderTargetFlags[0] = ViewFamily.renderTarget->GetDesc().Flags;
    graphicsPSOInit.NumSamples = 1;
    graphicsPSOInit.DepthStencilTargetFormat = PF_Unknown;

    graphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI().get();
    graphicsPSOInit.DepthStencilState = TStaticDepthStencilState<>::GetRHI().get();

    graphicsPSOInit.SubpassHint = SubpassHint::None;
    graphicsPSOInit.SubpassIndex = 0;

    graphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI().get();
    graphicsPSOInit.PrimitiveType = PT_TriangleList;

    // 遍历所有相机视口
    for (int32 index = 0; index < ViewFamily.views.size(); ++index)
    {
        auto context = GetDefaultContext();
    }
}

/// Helper function performing actual work in render thread.
/// @param SceneRenderers	List of scene renderers to use for rendering.
static void RenderViewFamilies(RHICommandListImmediate &RHICmdList, const std::vector<SceneRenderer *> &SceneRenderers)
{
    for (SceneRenderer *sceneRenderer : SceneRenderers)
    {
        sceneRenderer->Render();
    }
}

void RendererModule::BeginRenderingViewFamily(SceneViewFamily *ViewFamily)
{
    Scene *const scene = (Scene *)ViewFamily->scene;

    if (scene)
    {
        std::vector<SceneRenderer *> sceneRenderers;
        std::vector<const SceneViewFamily *> ViewFamiliesConst;
        ViewFamiliesConst.push_back(ViewFamily);
        SceneRenderer::CreateSceneRenderers(ViewFamiliesConst, sceneRenderers);
        RenderViewFamilies(GRHICommandListExecutor.GetImmediateCommandList(), sceneRenderers);
    }
}