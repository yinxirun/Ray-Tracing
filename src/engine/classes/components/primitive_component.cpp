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

    virtual void DrawMesh(const MeshBatch &Mesh, float ScreenSize) final override
    {
        primitiveComponent->staticMeshes.push_back(StaticMeshBatch(primitiveComponent, Mesh));
        StaticMeshBatch *StaticMesh = &primitiveComponent->staticMeshes.back();
    }

private:
    PrimitiveComponent *primitiveComponent;
};

void PrimitiveComponent::AddStaticMeshes(RHICommandListBase &RHICmdList, Scene *Scene, std::vector<PrimitiveComponent *> SceneInfos, bool bCacheMeshDrawCommands)
{
    for (int i = 0; i < SceneInfos.size(); ++i)
    {
        PrimitiveComponent *SceneInfo = SceneInfos[i];
        // Cache the primitive's static mesh elements.
        BatchingSPDI BatchingSPDI(SceneInfo);
        SceneInfo->DrawStaticElements(&BatchingSPDI);
    }

    if (bCacheMeshDrawCommands)
    {
        CacheMeshDrawCommands(Scene, SceneInfos);
    }
}

void PrimitiveComponent::CacheMeshDrawCommands(Scene *Scene, std::vector<PrimitiveComponent *> SceneInfos)
{
    for (int i = 0; i < SceneInfos.size(); i++)
    {
        SceneInfos[i]->StaticMeshCommandInfos.push_back({});
    }

    for (int i = 0; i < EMeshPass::Num; ++i)
    {
        EMeshPass::Type PassType = (EMeshPass::Type)i;
        MeshPassProcessor *PassMeshProcessor = PassProcessorManager::CreateMeshPassProcessor(PassType, Scene);

        delete PassMeshProcessor;
    }
}
