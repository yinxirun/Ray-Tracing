#pragma once
#include "mesh_pass_processor.h"
#include "engine/scene_management.h"
#include <unordered_set>

class ViewInfo;
class StaticMeshBatch;

class MeshDrawCommandPassSetupTaskContext
{
public:
    const ViewInfo *View;
    const Scene *Scene;
    MeshPass::Type PassType;
    ExclusiveDepthStencil::Type BasePassDepthStencilAccess;
    ExclusiveDepthStencil::Type DefaultBasePassDepthStencilAccess;

    // Mesh pass processor.
    MeshPassProcessor *MeshPassProcessor;
    const std::vector<MeshBatchAndRelevance> *DynamicMeshElements;
    const std::vector<MeshPassMask> *DynamicMeshElementsPassRelevance;

    // Commands.
    int32 NumDynamicMeshElements;
    int32 NumDynamicMeshCommandBuildRequestElements;
    std::vector<VisibleMeshDrawCommand> MeshDrawCommands;
    std::vector<VisibleMeshDrawCommand> MobileBasePassCSMMeshDrawCommands;
    std::vector<const StaticMeshBatch *> DynamicMeshCommandBuildRequests;
    std::vector<MeshDrawCommand> MeshDrawCommandStorage;
    std::vector<GraphicsPipelineStateInitializer> PipelineStatePassSet;
    bool NeedsShaderInitialisation;

    // Resources preallocated on rendering thread.
    void *PrimitiveIdBufferData;
    int32 PrimitiveIdBufferDataSize;
    std::vector<VisibleMeshDrawCommand> TempVisibleMeshDrawCommands;
};

class MeshDrawCommandPass
{
public:
    /// Dispatch visible mesh draw command process task, which prepares this pass for drawing.
    /// This includes generation of dynamic mesh draw commands, draw sorting and draw merging.
    void DispatchPassSetup(Scene *scene, const ViewInfo *view, MeshPass::Type PassType,
                           ExclusiveDepthStencil::Type BasePassDepthStencilAccess,
                           MeshPassProcessor *meshPassProcessor,
                           const std::vector<MeshBatchAndRelevance> &DynamicMeshElements,
                           const std::vector<MeshPassMask> *DynamicMeshElementsPassRelevance,
                           int32 NumDynamicMeshElements,
                           std::vector<const StaticMeshBatch *> &InOutDynamicMeshCommandBuildRequests,
                           int32 NumDynamicMeshCommandBuildRequestElements,
                           std::vector<VisibleMeshDrawCommand> &InOutMeshDrawCommands);

private:
    MeshDrawCommandPassSetupTaskContext TaskContext;

    // Maximum number of draws for this pass. Used to prealocate resources on rendering thread.
    // Has a guarantee that if there won't be any draws, then MaxNumDraws = 0;
    int32 MaxNumDraws;
};