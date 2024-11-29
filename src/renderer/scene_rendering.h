#pragma once
#include <vector>
#include "engine/scene_interface.h"
#include "renderer/internal/scene_textures.h"
#include "renderer/mesh_draw_commands.h"
#include "renderer/mesh_pass_processor.h"
#include "engine/mesh_batch.h"
#include "engine/scene_view.h"

class ViewCommands;
class IVisibilityTaskData;

/// View family plus associated transient scene textures.
/// 渲染模块
class CameraInfo : public Camera
{
public:
    explicit CameraInfo(const Camera &InViewFamily);

    inline SceneTextures &GetSceneTextures() { return sceneTextures; }

private:
    SceneTextures sceneTextures;
};

class ViewInfo : public SceneView
{
public:
    std::vector<MeshBatch> DynamicMeshElements;
    std::array<MeshDrawCommandPass, EMeshPass::Num> MeshDrawCommandPasses;

    std::vector<bool> primitiveVisibilityMap;
};

/// Used as the scope for scene rendering functions.
/// It is initialized in the game thread by FSceneViewFamily::BeginRender, and then passed to the rendering thread.
/// The rendering thread calls Render(), and deletes the scene renderer when it returns.
class SceneRenderer : public SceneInterface
{
public:
    Scene *scene = nullptr;

    /** The view family being rendered.  This references the Views array. */
    CameraInfo camera;

    /** The views being rendered. */
    std::vector<ViewInfo> Views;

    SceneRenderer(const Camera *InViewFamily);
    ~SceneRenderer();
    /** Creates multiple scene renderers based on the current feature level.  All view families must point to the same Scene. */
    static void CreateSceneRenderers(std::vector<const Camera *>, std::vector<SceneRenderer *> &out);

    void BeginInitViews(IVisibilityTaskData *VisibilityTaskData);

    void Render(RHICommandListImmediate &RHICmdList);

    void SetupMeshPass(ViewInfo &View, ExclusiveDepthStencil::Type BasePassDepthStencilAccess, ViewCommands &ViewCommands);

    /** Called at the begin / finish of the scene render. */
    IVisibilityTaskData *OnRenderBegin(RHICommandListImmediate &RHICmdList);

    /** All views include main camera views and custom render pass views. */
    std::vector<ViewInfo *> AllViews;
};