#pragma once
#include <vector>
#include <memory>
#include "core/math/vec.h"
#include "RHI/RHIResources.h"

class SceneInterface;
class SceneRenderer;

/// A projection from scene space into a 2D screen region.
class SceneView
{
public:
    IntVec2 ViewRect;
    float WorldToMetersScale = 100;
    float FOV;
    float NearClippingDistance;
    Vec3 ViewLocation;
    Mat4 ViewRotation;

    float farClippingDistance;
};

/// A set of views into a scene which only have different view transforms and owner actors.
/// 引擎模块
class Camera
{
public:
    /** The views which make up the family. */
    std::vector<const SceneView *> views;

    /** The render target which the views are being rendered to. */
    const Texture *renderTarget;

    /** The scene being viewed. */
    SceneInterface *scene;

    bool bIsViewFamilyInfo = false;

    /// Only FSceneRenderer can copy a view family.
    Camera(const Camera &) = default;
    Camera() = default;

    // void SetSceneRenderer(SceneRenderer *NewSceneRenderer) { SceneRenderer = NewSceneRenderer; }

private:
    /** The scene renderer that is rendering this view family. This is only initialized in the rendering thread's copies of the FSceneViewFamily. */
    // SceneRenderer *SceneRenderer;
};