#include "scene_visibility.h"
#include "scene_rendering.h"
#include "scene_private.h"
#include "engine/primitive_view_relevance.h"
#include "engine/classes/components/primitive_component.h"
#include "engine/static_mesh_batch.h"
#include "RHI/RHICommandList.h"

const bool UseCachedCommands = true;

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
    std::vector<VisibleMeshDrawCommand> VisibleCachedDrawCommands[MeshPass::Num];
    std::vector<const StaticMeshBatch *> DynamicBuildRequests[MeshPass::Num];
    int32 NumDynamicBuildRequestElements[MeshPass::Num];
    bool bUseCachedMeshDrawCommands = UseCachedCommands;

    void AddCommandsForMesh(int32 PrimitiveIndex, const PrimitiveComponent *InPrimitive,
                            const StaticMeshBatchRelevance &StaticMeshRelevance,
                            const StaticMeshBatch &StaticMesh, const Scene &scene, MeshPass::Type PassType)
    {
        const bool bUseCachedMeshCommand = bUseCachedMeshDrawCommands && StaticMeshRelevance.bSupportsCachingMeshDrawCommands;

        if (bUseCachedMeshCommand)
        {
            const int32 StaticMeshCommandInfoIndex = 0;
            if (StaticMeshCommandInfoIndex)
            {
                const CachedMeshDrawCommandInfo &CachedMeshDrawCommand = InPrimitive->staticMeshCommandInfos[StaticMeshCommandInfoIndex];
                const CachedPassMeshDrawList &SceneDrawList = scene.CachedDrawLists[PassType];

                VisibleCachedDrawCommands[(uint32)PassType].emplace_back();
                VisibleMeshDrawCommand &NewVisibleMeshDrawCommand = VisibleCachedDrawCommands[(uint32)PassType].back();

                const MeshDrawCommand *MeshDrawCommand = CachedMeshDrawCommand.StateBucketId >= 0
                                                             ? nullptr
                                                             : &SceneDrawList.MeshDrawCommands[CachedMeshDrawCommand.CommandIndex];

                NewVisibleMeshDrawCommand.Setup(MeshDrawCommand);
            }
        }
        else
        {
            check(0);
            NumDynamicBuildRequestElements[PassType] += StaticMeshRelevance.NumElements;
            DynamicBuildRequests[PassType].push_back(&StaticMesh);
        }
    }
};

// packet是为了并行计算设计的，一个packet一个线程
struct RelevancePacket
{
    RelevancePacket(const Scene &scene, const ViewInfo &view, const ViewCommands &viewCommands)
        : scene(scene), View(view), viewCommands(viewCommands) {}
    DrawCommandRelevancePacket DrawCommandPacket;
    /// primitive的索引
    std::vector<int32> Input;
    void ComputeRelevance()
    {
        int32 NumVisibleStaticMeshElements = 0;
        for (int32 InputPrimsIndex = 0; InputPrimsIndex < Input.size(); InputPrimsIndex++)
        {
            int32 BitIndex = Input[InputPrimsIndex];
            PrimitiveComponent *primitive = scene.primitives[BitIndex].get();

            PrimitiveViewRelevance ViewRelevance = primitive->GetViewRelevance(&View);

            const bool bStaticRelevance = ViewRelevance.bStaticRelevance;
            const bool bDrawRelevance = ViewRelevance.bDrawRelevance;
            const bool bDynamicRelevance = ViewRelevance.bDynamicRelevance;

            if (bStaticRelevance && (bDrawRelevance))
            {
                const int32 NumStaticMeshes = primitive->staticMeshes.size();
                for (int32 MeshIndex = 0; MeshIndex < NumStaticMeshes; MeshIndex++)
                {
                    const StaticMeshBatchRelevance &staticMeshRelevance = primitive->staticMeshRelevances[MeshIndex];
                    const StaticMeshBatch &StaticMesh = primitive->staticMeshes[MeshIndex];

                    int8 StaticMeshLODIndex = staticMeshRelevance.LODIndex;

                    // 需要检查这个LOD是否被渲染。目前不支持多层LOD，就不检查了

                    // Mark static mesh as visible for rendering
                    if (staticMeshRelevance.bUseForMaterial && ViewRelevance.bRenderInMainPass)
                    {
                        DrawCommandPacket;
                        ++NumVisibleStaticMeshElements;
                    }
                }
            }
        }
    }
    void MarkRelevant() {}

    // 将对象内暂存的相关command拷贝到viewcommands中。viewcommands是输出。
    void Finalize()
    {
        ViewInfo &WriteView = const_cast<ViewInfo &>(View);
        ViewCommands &WriteViewCommands = const_cast<ViewCommands &>(viewCommands);

        for (int32 PassIndex = 0; PassIndex < MeshPass::Num; PassIndex++)
        {
            std::vector<VisibleMeshDrawCommand> &SrcCommands = DrawCommandPacket.VisibleCachedDrawCommands[PassIndex];
            std::vector<VisibleMeshDrawCommand> &DstCommands = WriteViewCommands.MeshCommands[PassIndex];
            if (SrcCommands.size() > 0)
            {
                static_assert(sizeof(SrcCommands[0]) == sizeof(DstCommands[0]), "Memcpy sizes must match.");
                const int32 PrevNum = DstCommands.size();
                DstCommands.resize(DstCommands.size() + SrcCommands.size());
                memcpy(&DstCommands[PrevNum], &SrcCommands[0], SrcCommands.size() * sizeof(SrcCommands[0]));
            }

            // 暂时先省略不可缓存的静态网格相关的拷贝。

            WriteViewCommands.NumDynamicMeshCommandBuildRequestElements[PassIndex] +=
                DrawCommandPacket.NumDynamicBuildRequestElements[PassIndex];
        }
    }

    const Scene &scene;
    const ViewInfo &View;
    const ViewCommands &viewCommands;

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
        RelevancePacket *packet = new RelevancePacket(*scene, view, ViewCommands);
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
                    packet = new RelevancePacket(*scene, view, ViewCommands);
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
        for (int32 PassIndex = 0; PassIndex < MeshPass::Num; PassIndex++)
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
