#pragma once
#include "RHI/RHIResources.h"

class VertexFactory;

/// A batch mesh element definition.
struct MeshBatchElement
{
    UniformBuffer *primitiveUniformBuffer;

    /** The index buffer to draw the mesh batch with. */
    const Buffer *IndexBuffer;

    uint32 FirstIndex;
    /** When 0, IndirectArgsBuffer will be used. */
    uint32 NumPrimitives;

    /** Number of instances to draw.  If InstanceRuns is valid, this is actually the number of runs in InstanceRuns. */
    uint32 NumInstances = 0;
    uint32 BaseVertexIndex;
    uint32 MinVertexIndex;
    uint32 MaxVertexIndex;
};

/// A batch of mesh elements, all with the same material and vertex buffer
struct MeshBatch
{
    std::vector<MeshBatchElement> elements = std::vector<MeshBatchElement>(1);
    /** Vertex factory for rendering, required. */
    const VertexFactory *vertexFactory;

    /** LOD index of the mesh, used for fading LOD transitions. */
    int8 LODIndex;
    uint8 SegmentIndex;

    uint32 Type;
};