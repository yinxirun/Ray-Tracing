#include "engine/scene_interface.h"
#include "engine/scene_view.h"
#include "engine/classes/components/primitive_component.h"
#include "render_core/static_states.h"
#include "RHI/RHICommandList.h"
#include "RHI/RHIContext.h"
#include "RHI/dynamic_rhi.h"
#include "scene_rendering.h"
#include "renderer_module.h"
#include "scene_private.h"
#include "scene_visibility.h"
#include <memory>
#include <array>

CameraInfo::CameraInfo(const Camera &InViewFamily) : Camera(InViewFamily)
{
    bIsViewFamilyInfo = true;
}

SceneRenderer::SceneRenderer(const Camera *InViewFamily) : camera(*InViewFamily)
{
    AllViews.resize(InViewFamily->views.size());
    for (auto &viewInfo : AllViews)
    {
        viewInfo = new ViewInfo;
    }
}

SceneRenderer::~SceneRenderer()
{
    for (auto &viewInfo : AllViews)
    {
        delete viewInfo;
    }
}

void SceneRenderer::CreateSceneRenderers(std::vector<const Camera *> InViewFamilies, std::vector<SceneRenderer *> &out)
{
    const SceneInterface *scene = InViewFamilies[0]->scene;
    check(scene);
    for (int32 FamilyIndex = 0; FamilyIndex < InViewFamilies.size(); FamilyIndex++)
    {
        const Camera *InViewFamily = InViewFamilies[FamilyIndex];
        check(InViewFamily);
        check(InViewFamily->scene == scene);
        auto renderer = new SceneRenderer(InViewFamily);
        out.push_back(renderer);
    }
}

void SceneRenderer::BeginInitViews(IVisibilityTaskData *VisibilityTaskData)
{
    VisibilityTaskData->ProcessTasks();
}

void SceneRenderer::SetupMeshPass(ViewInfo &View, ExclusiveDepthStencil::Type BasePassDepthStencilAccess, ViewCommands &ViewCommands)
{
    for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; PassIndex++)
    {
        const EMeshPass::Type PassType = (EMeshPass::Type)PassIndex;
        MeshPassProcessor *meshPassProcessor = PassProcessorManager::CreateMeshPassProcessor(PassType, scene);
        MeshDrawCommandPass pass = View.MeshDrawCommandPasses[PassIndex];

        //pass.DispatchPassSetup();

        delete meshPassProcessor;
    }
}

void SceneRenderer::Render(RHICommandListImmediate &RHICmdList)
{
    // IVisibilityTaskData *VisibilityTaskData = OnRenderBegin(RHICmdList);
    //  CommitFinalPipelineState
    //  GSystemTextures.InitializeTextures
    //  FSceneTextures::InitializeViewFamily
    // BeginInitViews(VisibilityTaskData);
    // VisibilityTaskData->FinishGatherDynamicMeshElements();
    //  EndInitViews

    // 初始化视图：进行剔除，计算相关性
    {
        for (ViewInfo &viewInfo : Views)
        {
            viewInfo.primitiveVisibilityMap.assign(scene->primitives.size(), true);
        }
    }

    GraphicsPipelineStateInitializer graphicsPSOInit;
    graphicsPSOInit.RenderTargetsEnabled = 1;
    graphicsPSOInit.RenderTargetFormats[0] = camera.renderTarget->GetDesc().Format;
    graphicsPSOInit.RenderTargetFlags[0] = camera.renderTarget->GetDesc().Flags;
    graphicsPSOInit.NumSamples = 1;
    graphicsPSOInit.DepthStencilTargetFormat = PF_Unknown;

    graphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI().get();
    graphicsPSOInit.DepthStencilState = TStaticDepthStencilState<>::GetRHI().get();

    graphicsPSOInit.SubpassHint = SubpassHint::None;
    graphicsPSOInit.SubpassIndex = 0;

    graphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI().get();
    graphicsPSOInit.PrimitiveType = PT_TriangleList;

    // 遍历所有相机视口
    for (int32 index = 0; index < camera.views.size(); ++index)
    {
        auto context = GetDefaultContext();
    }
}

/// @todo 未完成 太难了
IVisibilityTaskData *SceneRenderer::OnRenderBegin(RHICommandListImmediate &RHICmdList)
{
    IVisibilityTaskData *VisibilityTaskData = nullptr;

    Scene::UpdateParameters SceneUpdateParameters;
    SceneUpdateParameters.Callbacks.PostStaticMeshUpdate = [&](std::function<void()> &StaticMeshUpdateTask)
    {
        VisibilityTaskData = LaunchVisibilityTasks(RHICmdList, *this, StaticMeshUpdateTask);
    };

    return VisibilityTaskData;
}

/// Helper function performing actual work in render thread.
/// @param SceneRenderers	List of scene renderers to use for rendering.
static void RenderViewFamilies(RHICommandListImmediate &RHICmdList, const std::vector<SceneRenderer *> &SceneRenderers)
{
    for (SceneRenderer *sceneRenderer : SceneRenderers)
    {
        sceneRenderer->Render(RHICmdList);
    }
}

void RendererModule::BeginRenderingViewFamily(Camera *ViewFamily)
{
    Scene *const scene = (Scene *)ViewFamily->scene;

    if (scene)
    {
        std::vector<SceneRenderer *> sceneRenderers;
        std::vector<const Camera *> ViewFamiliesConst;
        ViewFamiliesConst.push_back(ViewFamily);
        SceneRenderer::CreateSceneRenderers(ViewFamiliesConst, sceneRenderers);
        RenderViewFamilies(GRHICommandListExecutor.GetImmediateCommandList(), sceneRenderers);
    }
}
