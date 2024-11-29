#include "engine/classes/components/static_mesh_component.h"
#include "engine/mesh_batch.h"

/// Sets up a FMeshBatch for a specific LOD and element.
bool StaticMeshComponent::GetMeshElement(int32 lodIndex, int32 sectionIndex, MeshBatch &outMeshBatch)
{
    const StaticMeshLODResources &LOD = staticMesh->lod1;
    const StaticMeshVertexFactories &VFs = staticMesh->LODVertexFactories[lodIndex];
    const StaticMeshSection &section = LOD.sections[sectionIndex];

    if (section.numTriangles == 0)
        return false;

    const VertexFactory *vertexFactory = nullptr;
    MeshBatchElement &OutMeshBatchElement = outMeshBatch.elements[0];

    vertexFactory = &VFs.vertexFactory;

    const bool bWireframe = false;

    const uint32 NumPrimitives = SetMeshElementGeometrySource(lodIndex, sectionIndex, bWireframe, vertexFactory, outMeshBatch);

    if (NumPrimitives > 0)
    {
        outMeshBatch.SegmentIndex = sectionIndex;
        outMeshBatch.LODIndex = lodIndex;
        return true;
    }
    else
    {
        return false;
    }
}

uint32 StaticMeshComponent::SetMeshElementGeometrySource(
    int32 LODIndex,
    int32 SectionIndex,
    bool bWireframe,
    const VertexFactory *vertexFactory,
    MeshBatch &OutMeshBatch) const
{
    const StaticMeshLODResources &LOD = staticMesh->lod1;
    const StaticMeshSection &section = LOD.sections[SectionIndex];
    MeshBatchElement &OutMeshBatchElement = OutMeshBatch.elements[0];
    uint32 NumPrimitives = 0;
    if (bWireframe)
    {
        assert(0);
    }
    else
    {
        OutMeshBatch.Type = PT_TriangleList;
        NumPrimitives = section.numTriangles;
    }
    OutMeshBatchElement.NumPrimitives = NumPrimitives;
    OutMeshBatch.vertexFactory = vertexFactory;

    return NumPrimitives;
}

void StaticMeshComponent::GetDynamicMeshElements(const std::vector<const SceneView *> &Views, const Camera &ViewFamily, uint32 VisibilityMap, MeshElementCollector &Collector) const
{
    assert(0);
}

void StaticMeshComponent::DrawStaticElements(StaticPrimitiveDrawInterface *PDI)
{
    int numLOD = 1;
    for (int32 LODIndex = 0; LODIndex < numLOD; ++LODIndex)
    {
        const StaticMeshLODResources &LODModel = staticMesh->lod1;
        for (int32 SectionIndex = 0; SectionIndex < LODModel.sections.size(); SectionIndex++)
        {
            MeshBatch BaseMeshBatch;
            if (GetMeshElement(LODIndex, SectionIndex, BaseMeshBatch))
            {
                {
                    // Standard mesh elements.
                    // If we have submitted an optimized shadow-only mesh, remaining mesh elements must not cast shadows.
                    MeshBatch MeshBatch(BaseMeshBatch);
                    PDI->DrawMesh(MeshBatch, -1);
                }
            }
        }
    }
}

StaticMeshComponent::LODInfo::LODInfo(StaticMesh *mesh, int32 InLODIndex)
{
    Sections.resize(mesh->lod1.sections.size());

    for (int sectionIndex = 0; sectionIndex < Sections.size(); ++sectionIndex)
    {
    }
}