#pragma once

#include "mesh_component.h"
#include "engine/classes/static_mesh.h"
#include "engine/scene_management.h"
#include <memory>
#include <vector>

struct MeshBatch;
class VertexFactory;
class MaterialInterface;
class StaticPrimitiveDrawInterface;

/**
 * StaticMeshComponent is used to create an instance of a UStaticMesh.
 * A static mesh is a piece of geometry that consists of a static set of polygons.
 */
class StaticMeshComponent : public MeshComponent
{
private:
    std::shared_ptr<StaticMesh> staticMesh;

public:
    virtual bool SetStaticMesh(std::shared_ptr<StaticMesh> NewMesh);

    /********************************* Proxy *********************************/
public:
    virtual bool GetMeshElement(int32 lodIndex, int32 sectionIndex, MeshBatch &outMeshBatch);

    /** Configures mesh batch vertex / index state. Returns the number of primitives used in the element. */
    uint32 SetMeshElementGeometrySource(
        int32 LODIndex,
        int32 ElementIndex,
        bool bWireframe,
        const VertexFactory *vertexFactory,
        MeshBatch &OutMeshElement) const;

    virtual void GetDynamicMeshElements(const std::vector<const SceneView *> &Views, const SceneViewFamily &ViewFamily, uint32 VisibilityMap, MeshElementCollector &Collector) const override;

    virtual void DrawStaticElements(StaticPrimitiveDrawInterface *PDI) override;

protected:
    /** Information used by the proxy about a single LOD of the mesh. */
    class LODInfo : public LightCacheInterface
    {
    public:
        /** Information about an element of a LOD. */
        struct SectionInfo
        {
            /** Default constructor. */
            SectionInfo() : Material(nullptr), FirstPreCulledIndex(0), NumPreCulledTriangles(-1) {}

            /** The material with which to render this section. */
            MaterialInterface *Material;

            int32 FirstPreCulledIndex;
            int32 NumPreCulledTriangles;
        };

        /** Per-section information. */
        std::vector<SectionInfo> Sections;

        /** Initialization constructor. */
        LODInfo(StaticMesh *mesh, int32 InLODIndex);
    };
};