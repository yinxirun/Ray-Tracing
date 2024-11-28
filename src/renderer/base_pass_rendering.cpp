#include "base_pass_rendering.h"
#include "RHI/RHIResources.h"
#include "RHI/dynamic_rhi.h"
#include <memory>

VertexShader *vs;
PixelShader *ps;
extern std::vector<uint8> process_shader(std::string spvFilename, ShaderFrequency freq);

ForwardPassMeshProcessor::ForwardPassMeshProcessor(EMeshPass::Type InMeshPassType, const Scene *Scene)
    : MeshPassProcessor(InMeshPassType, Scene)
{
}

void ForwardPassMeshProcessor::ForwardPassMeshProcessor::AddMeshBatch(const MeshBatch &meshBatch, uint64 BatchElementMask,
                                                                      const PrimitiveComponent *PrimitiveSceneProxy, int32 StaticMeshId)
{
    MeshProcessorShaders shaders;
    shaders.vertexShader = CreateVertexShader(process_shader("shaders/a.vert.spv", SF_Vertex));
    shaders.pixelShader = CreatePixelShader(process_shader("shaders/a.frag.spv", SF_Pixel));
    const RasterizerFillMode MeshFillMode = FM_Solid;
    const RasterizerCullMode MeshCullMode = CM_None;
    BuildMeshDrawCommands(meshBatch, state, shaders, MeshFillMode, MeshCullMode);
}