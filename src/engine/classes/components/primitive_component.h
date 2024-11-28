#pragma once
#include <vector>
#include "definitions.h"
#include "engine/static_mesh_batch.h"
#include "renderer/mesh_pass_processor.h"

class SceneView;
class SceneViewFamily;
class Scene;
class StaticPrimitiveDrawInterface;
class MeshElementCollector;

class RHICommandListBase;

/**
 * PrimitiveComponents are SceneComponents that contain or generate some sort of geometry, generally to be rendered or used as collision data.
 * There are several subclasses for the various types of geometry, but the most common by far are the ShapeComponents (Capsule, Sphere, Box), StaticMeshComponent, and SkeletalMeshComponent.
 * ShapeComponents generate geometry that is used for collision detection but are not rendered, while StaticMeshComponents and SkeletalMeshComponents contain pre-built geometry that is rendered, but can also be used for collision detection.
 */
class PrimitiveComponent
{
    /********************************* Proxy *********************************/
public:
    /**
     * Gathers the primitive's dynamic mesh elements.  This will only be called if GetViewRelevance declares dynamic relevance.
     * This is called from the rendering thread for each set of views that might be rendered.
     * Game thread state like UObjects must have their properties mirrored on the proxy to avoid race conditions.  The rendering thread must not dereference UObjects.
     * The gathered mesh elements will be used multiple times, any memory referenced must last as long as the Collector (eg no stack memory should be referenced).
     * This function should not modify the proxy but simply collect a description of things to render.  Updates to the proxy need to be pushed from game thread or external events.
     *
     * @param Views - the array of views to consider.  These may not exist in the ViewFamily.
     * @param ViewFamily - the view family, for convenience
     * @param VisibilityMap - a bit representing this proxy's visibility in the Views array
     * @param Collector - gathers the mesh elements to be rendered and provides mechanisms for temporary allocations
     */
    virtual void GetDynamicMeshElements(const std::vector<const SceneView *> &Views, const SceneViewFamily &ViewFamily, uint32 VisibilityMap, MeshElementCollector &Collector) const {}

    /**
     * Draws the primitive's static elements.  This is called from the rendering thread once when the scene proxy is created.
     * The static elements will only be rendered if GetViewRelevance declares static relevance.
     * @param PDI - The interface which receives the primitive elements.
     */
    virtual void DrawStaticElements(StaticPrimitiveDrawInterface *PDI) {}

    /********************************* SceneInfo *********************************/

    std::vector<StaticMeshBatch> staticMeshes;

    /** The primitive's cached mesh draw commands infos for all static meshes. Kept separately from StaticMeshes for cache efficiency inside InitViews. */
    std::vector<CachedMeshDrawCommandInfo> StaticMeshCommandInfos;

    /** Adds the primitive's static meshes to the scene. */
    static void AddStaticMeshes(RHICommandListBase &RHICmdList, Scene *Scene, std::vector<PrimitiveComponent *> SceneInfos, bool bCacheMeshDrawCommands = true);

    /** Creates cached mesh draw commands for all meshes. */
    static void CacheMeshDrawCommands(Scene *Scene, std::vector<PrimitiveComponent *> SceneInfos);
};