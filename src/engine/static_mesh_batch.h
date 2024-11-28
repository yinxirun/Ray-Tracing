#pragma once

#include "engine/mesh_batch.h"

class PrimitiveComponent;

/**
 * A mesh which is defined by a primitive at scene segment construction time and never changed.
 * Lights are attached and detached as the segment containing the mesh is added or removed from a scene.
 */
class StaticMeshBatch : public MeshBatch
{
public:
    /** The render info for the primitive which created this mesh. */
    PrimitiveComponent *PrimitiveSceneInfo;

    /** The index of the mesh in the scene's static meshes array. */
    int32 Id;

    // Constructor/destructor.
    StaticMeshBatch(PrimitiveComponent *InPrimitiveSceneInfo, const MeshBatch &InMesh);
    ~StaticMeshBatch();

/* private: */
    /** Private copy constructor. */
    StaticMeshBatch(const StaticMeshBatch &InStaticMesh);
};