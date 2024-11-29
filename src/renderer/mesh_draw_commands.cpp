#include "mesh_draw_commands.h"
#include "scene_private.h"

/// Converts each FMeshBatch into a set of FMeshDrawCommands for a specific mesh pass type.
void GenerateDynamicMeshDrawCommands(const ViewInfo &View,
                                     EMeshPass::Type PassType,
                                     MeshPassProcessor *PassMeshProcessor,
                                     const std::vector<MeshBatchAndRelevance> &DynamicMeshElements,
                                     const std::vector<MeshPassMask> *DynamicMeshElementsPassRelevance,
                                     int32 MaxNumDynamicMeshElements,
                                     const std::vector<const StaticMeshBatch *> &DynamicMeshCommandBuildRequests,
                                     int32 MaxNumBuildRequestElements,
                                     std::vector<VisibleMeshDrawCommand> &VisibleCommands,
                                     std::vector<MeshDrawCommand> &MeshDrawCommandStorage,
                                     GraphicsPipelineStateInitializer &MinimalPipelineStatePassSet,
                                     bool &NeedsShaderInitialisation)
{
}

/*
如果我没猜错的话，InOutMeshDrawCommands是纯静态网格，InOutDynamicMeshCommandBuildRequests是依赖View的静态网格，DynamicMeshElements是动态网格
*/
void MeshDrawCommandPass::DispatchPassSetup(Scene *scene, const ViewInfo *view, EMeshPass::Type PassType,
                                            ExclusiveDepthStencil::Type BasePassDepthStencilAccess,
                                            MeshPassProcessor *meshPassProcessor,
                                            const std::vector<MeshBatchAndRelevance> &DynamicMeshElements,
                                            const std::vector<MeshPassMask> *DynamicMeshElementsPassRelevance,
                                            int32 NumDynamicMeshElements,
                                            std::vector<const StaticMeshBatch *> &InOutDynamicMeshCommandBuildRequests,
                                            int32 NumDynamicMeshCommandBuildRequestElements,
                                            std::vector<VisibleMeshDrawCommand> &InOutMeshDrawCommands)
{
    MaxNumDraws = InOutMeshDrawCommands.size() + NumDynamicMeshElements + NumDynamicMeshCommandBuildRequestElements;

    TaskContext.MeshPassProcessor = meshPassProcessor;
    TaskContext.DynamicMeshElements = &DynamicMeshElements;
    TaskContext.DynamicMeshElementsPassRelevance = DynamicMeshElementsPassRelevance;

    TaskContext.View = view;
    TaskContext.PassType = PassType;

    TaskContext.BasePassDepthStencilAccess = BasePassDepthStencilAccess;
    TaskContext.DefaultBasePassDepthStencilAccess = scene->DefaultBasePassDepthStencilAccess;
    TaskContext.NumDynamicMeshElements = NumDynamicMeshElements;
    TaskContext.NumDynamicMeshCommandBuildRequestElements = NumDynamicMeshCommandBuildRequestElements;

    std::swap(TaskContext.MeshDrawCommands, InOutMeshDrawCommands);
    std::swap(TaskContext.DynamicMeshCommandBuildRequests, InOutDynamicMeshCommandBuildRequests);

    if (MaxNumDraws > 0)
    {
        TaskContext.MeshDrawCommands.reserve(MaxNumDraws);
    }
}