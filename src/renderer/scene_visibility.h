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

class IVisibilityTaskData
{
public:
    virtual void ProcessTasks() = 0;
};

////////////////////////////////////////////////////////////////////////// Private
class VisibilityTaskData;
class ViewInfo;
class VisibilityViewPacket;
class RHICommandListImmediate;
class SceneRenderer;

class VisibilityViewPacket
{
public:
    VisibilityViewPacket(VisibilityTaskData &TaskData, Scene &InScene, ViewInfo &InView, int32 ViewIndex);
};

class VisibilityTaskData : public IVisibilityTaskData
{
private:
    RHICommandListImmediate &RHICmdList;
    SceneRenderer &sceneRenderer;
    Scene &scene;
    std::vector<ViewInfo *> Views;
    std::vector<VisibilityViewPacket> ViewPackets;

    struct DynamicMeshElements
    {
        std::vector<ViewCommands> ViewCommandsPerView;
    } DynamicMeshElements;

    struct Tasks
    { // These legacy tasks are used to interface with the jobs launched prior to gather dynamic mesh elements.
    } Tasks;

public:
    VisibilityTaskData(RHICommandListImmediate &RHICmdList, SceneRenderer &SceneRenderer);
    void LaunchVisibilityTasks(std::function<void()> &BeginInitVisibilityPrerequisites);
    void ProcessTasks()
    {
    }
};

class SceneRenderer;
extern IVisibilityTaskData *LaunchVisibilityTasks(RHICommandListImmediate &RHICmdList,
                                                  SceneRenderer &SceneRenderer,
                                                  std::function<void()> &BeginInitVisibilityTaskPrerequisites);
