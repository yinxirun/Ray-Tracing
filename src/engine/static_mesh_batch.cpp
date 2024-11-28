#include "static_mesh_batch.h"

StaticMeshBatch::StaticMeshBatch(PrimitiveComponent *InPrimitiveSceneInfo, const MeshBatch &InMesh)
    : MeshBatch(InMesh), PrimitiveSceneInfo(InPrimitiveSceneInfo), Id(-1)
{
}

/** Private copy constructor. */
StaticMeshBatch::StaticMeshBatch(const StaticMeshBatch &InStaticMesh)
    : MeshBatch(InStaticMesh), PrimitiveSceneInfo(InStaticMesh.PrimitiveSceneInfo), Id(InStaticMesh.Id)
{
}

StaticMeshBatch::~StaticMeshBatch()
{
}