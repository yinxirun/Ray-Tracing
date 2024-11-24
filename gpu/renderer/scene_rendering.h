#pragma once
#include <vector>
#include "gpu/engine/scene_interface.h"

/// View family plus associated transient scene textures.
/// 渲染模块
class ViewFamilyInfo : public SceneViewFamily
{
public:
    explicit ViewFamilyInfo(const SceneViewFamily &InViewFamily);
};

/// Used as the scope for scene rendering functions.
/// It is initialized in the game thread by FSceneViewFamily::BeginRender, and then passed to the rendering thread.
/// The rendering thread calls Render(), and deletes the scene renderer when it returns.
class SceneRenderer : public SceneInterface
{
public:
    /** The view family being rendered.  This references the Views array. */
    ViewFamilyInfo ViewFamily;

    SceneRenderer(const SceneViewFamily *InViewFamily);
    /** Creates multiple scene renderers based on the current feature level.  All view families must point to the same Scene. */
    static void CreateSceneRenderers(std::vector<const SceneViewFamily *>, std::vector<SceneRenderer *> &out);

    void Render();
};