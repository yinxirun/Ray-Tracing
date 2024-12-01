#pragma once

#include "engine/mesh_batch.h"
#include "renderer/mesh_pass_processor.h"

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
    /// Private copy constructor.
    StaticMeshBatch(const StaticMeshBatch &InStaticMesh);
};

class StaticMeshBatchRelevance
{
public:
    StaticMeshBatchRelevance(const StaticMeshBatch &StaticMesh, bool InbSupportsCachingMeshDrawCommands);

    int8 LODIndex;

    /// Starting offset into continuous array of command infos for this mesh in FPrimitiveSceneInfo::CachedMeshDrawCommandInfos.
    MeshPassMask CommandInfosMask;

    /// Number of elements in this mesh.
    uint16 NumElements;

    /// Every bit corresponds to one MeshPass.
    /// If bit is set, then FPrimitiveSceneInfo::CachedMeshDrawCommandInfos contains this mesh pass.
    uint16 CommandInfosBase;

    uint8 bUseForMaterial : 1; // Whether it can be used in renderpasses requiring material outputs.

    /// Cached from vertex factory to avoid dereferencing VF in InitViews.
    uint8 bSupportsCachingMeshDrawCommands : 1;
};