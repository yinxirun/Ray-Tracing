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
///////////////////////////////////////////////////////////////////////////////////
struct DrawCommandRelevancePacket
{
    std::vector<VisibleMeshDrawCommand> VisibleCachedDrawCommands[EMeshPass::Num];
    std::vector<const StaticMeshBatch *> DynamicBuildRequests[EMeshPass::Num];
    int32 NumDynamicBuildRequestElements[EMeshPass::Num];
    bool bUseCachedMeshDrawCommands;
};
struct RelevancePacket
{
    DrawCommandRelevancePacket DrawCommandPacket;
    std::vector<int32> Input;
    void ComputeRelevance()
    {
        for (int32 Index = 0; Index < Input.size(); Index++)
        {
            
        }
    }
    void MarkRelevant() {}
    void Finalize() {}

    static const int32 MaxOutputPrims;
};
const int32 RelevancePacket::MaxOutputPrims = 128;

static void ComputeAndMarkRelevanceForView(const Scene *scene, ViewInfo &view, ViewCommands &ViewCommands,
                                           uint8 viewBit)
{
    int32 NumMesh = view.StaticMeshVisibilityMap.size();
    std::vector<RelevancePacket *> Packets;

    int BitIt = -1;
    for (int i = 0; i < view.primitiveVisibilityMap.size(); ++i)
    {
        if (view.primitiveVisibilityMap[i])
            break;
    }

    if (BitIt != -1)
    {
        RelevancePacket *packet = new RelevancePacket;
        Packets.push_back(packet);
        while (true)
        {
            for (; BitIt < view.primitiveVisibilityMap.size(); ++BitIt)
            {
                if (view.primitiveVisibilityMap[BitIt])
                    break;
            }

            if (packet->Input.size() >= RelevancePacket::MaxOutputPrims ||
                BitIt == view.primitiveVisibilityMap.size())
            {
                if (BitIt == view.primitiveVisibilityMap.size())
                    break;
                else
                {
                    packet = new RelevancePacket;
                    Packets.push_back(packet);
                }
            }
        }
    }

    // 这里可以并行
    for (RelevancePacket *pack : Packets)
    {
        pack->ComputeRelevance();
        pack->MarkRelevant();
    }

    {
        for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; PassIndex++)
        {
            int32 NumVisibleCachedMeshDrawCommands = 0;
            int32 NumDynamicBuildRequests = 0;

            for (auto Packet : Packets)
            {
                NumVisibleCachedMeshDrawCommands += Packet->DrawCommandPacket.VisibleCachedDrawCommands[PassIndex].size();
                NumDynamicBuildRequests += Packet->DrawCommandPacket.DynamicBuildRequests[PassIndex].size();
            }
        }

        for (auto Packet : Packets)
        {
            Packet->Finalize();
            delete Packet;
        }
    }
}

void SceneRenderer::ComputeViewVisibility(ExclusiveDepthStencil::Type BasePassDepthStencilAccess,
                                          std::vector<ViewCommands> &ViewCommandsPerView)
{
    uint8 ViewBit = 0x1;
    for (int ViewIndex = 0; ViewIndex < Views.size(); ++ViewIndex, ViewBit << 1)
    {
        ViewInfo &view = Views[ViewIndex];
        ViewCommands &viewCommands = ViewCommandsPerView[ViewIndex];
        ComputeAndMarkRelevanceForView(scene, view, viewCommands, ViewBit);
    }

    GatherDynamicMeshElements(Views, scene);

    for (int ViewIndex = 0; ViewIndex < Views.size(); ++ViewIndex)
    {
        ViewInfo &view = Views[ViewIndex];
        ViewCommands &viewCommands = ViewCommandsPerView[ViewIndex];

        SetupMeshPass(view, BasePassDepthStencilAccess, viewCommands);
    }
}
