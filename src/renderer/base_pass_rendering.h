#pragma once

#include "renderer/mesh_pass_processor.h"

class MeshBatch;
class PrimitiveComponent;

class ForwardPassMeshProcessor : public MeshPassProcessor
{
public:
    ForwardPassMeshProcessor(MeshPass::Type InMeshPassType, const Scene *Scene);

    void AddMeshBatch(const MeshBatch &meshBatch, uint64 BatchElementMask,
                      const PrimitiveComponent *PrimitiveSceneProxy, int32 StaticMeshId);

private:
    MeshPassProcessorRenderState state;
};