#include "engine/scene_interface.h"
#include "engine/scene_view.h"
#include "RHI/RHICommandList.h"
#include "scene_rendering.h"
#include "render_module.h"
#include "scene_private.h"
#include <memory>

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

void SceneRenderer::Render()
{
    
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