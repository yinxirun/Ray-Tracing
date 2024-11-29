#pragma once

class MeshBatch;

/// An interface to cached lighting for a specific mesh.
class LightCacheInterface
{
};

/// Encapsulates the gathering of meshes from the various FPrimitiveSceneProxy classes.
class MeshElementCollector
{
};

/**
 * An interface used to query a primitive for its static elements.
 */
class StaticPrimitiveDrawInterface
{
public:
    virtual ~StaticPrimitiveDrawInterface() {}

    virtual void DrawMesh(const MeshBatch &Mesh, float ScreenSize) = 0;
};

/// A reference to a mesh batch that is added to the collector, together with some cached relevance flags.
struct MeshBatchAndRelevance
{
    const MeshBatch *Mesh;
    const PrimitiveComponent *primitiveComponent;
};