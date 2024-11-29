#include "scene_visibility.h"
#include "scene_rendering.h"
#include "RHI/RHICommandList.h"

IVisibilityTaskData *LaunchVisibilityTasks(RHICommandListImmediate &RHICmdList, SceneRenderer &SceneRenderer, std::function<void()> &BeginInitVisibilityTaskPrerequisites)
{
    VisibilityTaskData *TaskData = new VisibilityTaskData(RHICmdList, SceneRenderer);
    TaskData->LaunchVisibilityTasks(BeginInitVisibilityTaskPrerequisites);
    return TaskData;
}

VisibilityViewPacket::VisibilityViewPacket(VisibilityTaskData &TaskData, Scene &InScene, ViewInfo &InView, int32 ViewIndex)
{
}

VisibilityTaskData::VisibilityTaskData(RHICommandListImmediate &RHICmdList, SceneRenderer &SceneRenderer)
    : RHICmdList(RHICmdList), sceneRenderer(SceneRenderer), Views(SceneRenderer.AllViews), scene(*sceneRenderer.scene)
{
}

void VisibilityTaskData::LaunchVisibilityTasks(std::function<void()> &BeginInitVisibilityPrerequisites)
{
    for (int32 ViewIndex = 0; ViewIndex < Views.size(); ++ViewIndex)
    {
        // Each view gets its own visibility task packet which contains all the state to manage the task graph for a view.
        ViewPackets.push_back(VisibilityViewPacket(*this, scene, *Views[ViewIndex], ViewIndex));
    }

    DynamicMeshElements.ViewCommandsPerView.resize(Views.size());
}
