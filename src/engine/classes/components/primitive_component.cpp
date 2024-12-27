#include "primitive_component.h"
#include "engine/scene_management.h"
#include "engine/mesh_batch.h"
#include "engine/static_mesh_batch.h"
#include "renderer/mesh_pass_processor.h"

/** An implementation of FStaticPrimitiveDrawInterface that stores the drawn elements for the rendering thread to use. */
class BatchingSPDI : public StaticPrimitiveDrawInterface
{
public:
    BatchingSPDI(PrimitiveComponent *primitiveComponent) : primitiveComponent(primitiveComponent) {}

    // 将静态网格的batch(以及其对应的 static mesh relevance)保存到primitive
    virtual void DrawMesh(const MeshBatch &Mesh, float ScreenSize) final override
    {
        primitiveComponent->staticMeshes.emplace_back(primitiveComponent, Mesh);
        StaticMeshBatch *StaticMesh = &primitiveComponent->staticMeshes.back();

        const bool bSupportsCachingMeshDrawCommands = (StaticMesh->elements.size() == 1);

        primitiveComponent->staticMeshRelevances.emplace_back(*StaticMesh, bSupportsCachingMeshDrawCommands);
    }

private:
    PrimitiveComponent *primitiveComponent;
};

void PrimitiveComponent::AddStaticMeshes(RHICommandListBase &RHICmdList, Scene *Scene, std::vector<PrimitiveComponent *> primitives, bool bCacheMeshDrawCommands)
{
    for (int i = 0; i < primitives.size(); ++i)
    {
        PrimitiveComponent *primitive = primitives[i];
        // Cache the primitive's static mesh elements.
        BatchingSPDI BatchingSPDI(primitive);
        primitive->DrawStaticElements(&BatchingSPDI);
    }

    if (bCacheMeshDrawCommands)
    {
        CacheMeshDrawCommands(Scene, primitives);
    }
}

void PrimitiveComponent::CacheMeshDrawCommands(Scene *Scene, std::vector<PrimitiveComponent *> primitives)
{
    CachedPassMeshDrawListContextImmediate DrawListContext(*Scene);

    // 表达第几个primitive上的第几个meshbatch
    struct MeshInfoAndIndex
    {
        int32 InfoIndex;
        int32 MeshIndex;
    };
    std::vector<MeshInfoAndIndex> MeshBatches;

    for (int i = 0; i < primitives.size(); i++)
    {
        primitives[i]->staticMeshCommandInfos.resize(MeshPass::Num * primitives[i]->staticMeshes.size());

        for (int32 MeshIndex = 0; MeshIndex < primitives[i]->staticMeshes.size(); MeshIndex++)
        {
            MeshBatches.push_back({i, MeshIndex});
        }
    }

    for (int i = 0; i < MeshPass::Num; ++i)
    {
        MeshPass::Type PassType = (MeshPass::Type)i;
        MeshPassProcessor *PassMeshProcessor = PassProcessorManager::CreateMeshPassProcessor(PassType, Scene, &DrawListContext);

        if (PassMeshProcessor != nullptr)
        {
            for (const MeshInfoAndIndex &MeshAndInfo : MeshBatches)
            {
                PrimitiveComponent *primitive = primitives[MeshAndInfo.InfoIndex];
                StaticMeshBatch &Mesh = primitive->staticMeshes[MeshAndInfo.MeshIndex];
                StaticMeshBatchRelevance &MeshRelevance = primitive->staticMeshRelevances[MeshAndInfo.MeshIndex];

                uint64 BatchElementMask = ~0ull;
                // NOTE: AddMeshBatch calls FCachedPassMeshDrawListContext::FinalizeCommand
                PassMeshProcessor->AddMeshBatch(Mesh, BatchElementMask, primitive);

                CachedMeshDrawCommandInfo CommandInfo = DrawListContext.GetCommandInfoAndReset();

                if (CommandInfo.CommandIndex != -1 || CommandInfo.StateBucketId != -1)
                {
                    MeshRelevance.CommandInfosMask.Set(PassType);
                    MeshRelevance.CommandInfosBase++;

                    int commandInfoIndex = MeshAndInfo.MeshIndex * MeshPass::Num + PassType;
                    primitive->staticMeshCommandInfos[commandInfoIndex] = CommandInfo;
                }
            }
        }

        delete PassMeshProcessor;
    }
}
