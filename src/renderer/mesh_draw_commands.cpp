#include "mesh_draw_commands.h"
#include "scene_private.h"
#include "engine/mesh_batch.h"
#include "engine/static_mesh_batch.h"

/// Converts each FMeshBatch into a set of MeshDrawCommands for a specific mesh pass type.
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
                                     std::vector<GraphicsPipelineStateInitializer> &MinimalPipelineStatePassSet,
                                     bool &NeedsShaderInitialisation)
{
    // movable
    {
        const int32 NumCommandsBefore = VisibleCommands.size();
        const int32 NumDynamicMeshBatches = DynamicMeshElements.size();
        for (int32 MeshIndex = 0; MeshIndex < NumDynamicMeshBatches; MeshIndex++)
        {
            if (!DynamicMeshElementsPassRelevance || (*DynamicMeshElementsPassRelevance)[MeshIndex].Get(PassType))
            {
                check(0);
            }
        }
    }

    // static but view dependent
    {
        const int32 NumCommandsBefore = VisibleCommands.size();
        const int32 NumStaticMeshBatches = DynamicMeshCommandBuildRequests.size();

        for (int32 MeshIndex = 0; MeshIndex < NumStaticMeshBatches; MeshIndex++)
        {
            const StaticMeshBatch *staticMeshBatch = DynamicMeshCommandBuildRequests[MeshIndex];
            const uint64 DefaultBatchElementMask = ~0ul;

            PassMeshProcessor->AddMeshBatch(*((const MeshBatch *)staticMeshBatch), DefaultBatchElementMask,
                                            staticMeshBatch->PrimitiveSceneInfo, staticMeshBatch->Id);
        }
    }
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

        GenerateDynamicMeshDrawCommands(*TaskContext.View,
                                        TaskContext.PassType,
                                        TaskContext.MeshPassProcessor,
                                        *TaskContext.DynamicMeshElements,
                                        TaskContext.DynamicMeshElementsPassRelevance,
                                        TaskContext.NumDynamicMeshElements,
                                        TaskContext.DynamicMeshCommandBuildRequests,
                                        TaskContext.NumDynamicMeshCommandBuildRequestElements,
                                        TaskContext.MeshDrawCommands,
                                        TaskContext.MeshDrawCommandStorage,
                                        TaskContext.PipelineStatePassSet,
                                        TaskContext.NeedsShaderInitialisation);
    }
}