#pragma once
#include "definitions.h"
#include "renderer/mesh_pass_processor.h"
#include "engine/static_mesh_batch.h"
class ViewCommands
{
    // public:
    //     ViewCommands()
    //     {
    //         for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; ++PassIndex)
    //         {
    //             NumDynamicMeshCommandBuildRequestElements[PassIndex] = 0;
    //         }
    //     }

    // TStaticArray<FMeshCommandOneFrameArray, EMeshPass::Num> MeshCommands;
    std::array<int32, EMeshPass::Num> NumDynamicMeshCommandBuildRequestElements;
    std::array<std::vector<const StaticMeshBatch *>, EMeshPass::Num> DynamicMeshCommandBuildRequests;
    // TStaticArray<TArray<EMeshDrawCommandCullingPayloadFlags, SceneRenderingAllocator>, EMeshPass::Num> DynamicMeshCommandBuildFlags;
};

class VisibilityTaskData
{
private:
    struct DynamicMeshElements
    {
        std::vector<ViewCommands> ViewCommandsPerView;
    } DynamicMeshElements;
};