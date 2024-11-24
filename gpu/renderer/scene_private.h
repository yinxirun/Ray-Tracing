#pragma once

#include "gpu_scene.h"
#include "gpu/engine/scene_interface.h"
#include <vector>
#include <unordered_set>
#include <memory>

class PrimitiveComponent;

/// Renderer scene which is private to the renderer module.
/// Ordinarily this is the renderer version of a UWorld, but an FScene can be created for previewing in editors which don't have a UWorld as well.
/// The scene stores renderer state that is independent of any view or frame, with the primary actions being adding and removing of primitives and lights.
/// 在UE中，有game thread和render thread。PrimitiveComponent由game thread掌握，Scene由render thread掌握，所以需要FPrimitiveSceneInfo和FPrimitiveSceneProxy作为中间人。在本项目中，没有game thread，所以做了简化。
class Scene : public SceneInterface
{
public:
    GPUScene GPUScene;

    /** Initialization constructor. */
    Scene();
    virtual ~Scene();

    // interface
    void AddPrimitive(std::shared_ptr<PrimitiveComponent> Primitive);
    void RemovePrimitive(std::shared_ptr<PrimitiveComponent> Primitive);

    std::unordered_set<std::shared_ptr<PrimitiveComponent>> addedPrimitive;
};