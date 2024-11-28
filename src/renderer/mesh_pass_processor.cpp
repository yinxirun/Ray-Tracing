#include "mesh_pass_processor.h"
#include "RHI/RHIResources.h"
#include "engine/mesh_batch.h"
#include "render_core/vertex_factory.h"
#include "render_core/static_states.h"
#include "renderer/scene_private.h"
#include "renderer/base_pass_rendering.h"

void MeshDrawCommand::SetDrawParametersAndFinalize(
    const MeshBatch &MeshBatch,
    int32 BatchElementIndex,
    GraphicsPipelineStateInitializer PipelineId)
{
    const MeshBatchElement &BatchElement = MeshBatch.elements[BatchElementIndex];

    /* check(!BatchElement.IndexBuffer || (BatchElement.IndexBuffer && BatchElement.IndexBuffer->IsInitialized() && BatchElement.IndexBuffer->IndexBufferRHI)); */

    IndexBuffer = const_cast<Buffer *>(BatchElement.IndexBuffer);
    FirstIndex = BatchElement.FirstIndex;
    NumPrimitives = BatchElement.NumPrimitives;

    if (NumPrimitives > 0)
    {
        VertexParams.BaseVertexIndex = BatchElement.BaseVertexIndex;
        VertexParams.NumVertices = BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1;
        /* checkf(!BatchElement.IndirectArgsBuffer, "FMeshBatchElement::NumPrimitives must be set to 0 when a IndirectArgsBuffer is used"); */
    }
    else
    {
        exit(0);
        /* checkf(BatchElement.IndirectArgsBuffer, TEXT("It is only valid to set BatchElement.NumPrimitives == 0 when a IndirectArgsBuffer is used"));
        IndirectArgs.Buffer = BatchElement.IndirectArgsBuffer;
        IndirectArgs.Offset = BatchElement.IndirectArgsOffset; */
    }

    PsoInit = PipelineId;
}

MeshDrawCommand &CachedPassMeshDrawListContext::AddCommand(MeshDrawCommand &Initializer, uint32 NumElements)
{
    MeshDrawCommandForStateBucketing = Initializer;
    return MeshDrawCommandForStateBucketing;
}

void CachedPassMeshDrawListContextImmediate::FinalizeCommand(
    const MeshBatch &MeshBatch,
    int32 BatchElementIndex,
    RasterizerFillMode MeshFillMode,
    RasterizerCullMode MeshCullMode,
    const GraphicsPipelineStateInitializer &PipelineState,
    MeshDrawCommand &MeshDrawCommand)
{
    MeshDrawCommand.SetDrawParametersAndFinalize(MeshBatch, BatchElementIndex, PipelineState);
    CommandInfo = CachedMeshDrawCommandInfo(CurrMeshPass);
    CommandInfo.MeshFillMode = MeshFillMode;
    CommandInfo.MeshCullMode = MeshCullMode;

    // Only one FMeshDrawCommand supported per FStaticMesh in a pass
    // Allocate at lowest free index so that 'r.DoLazyStaticMeshUpdate' can shrink the TSparseArray more effectively
    CachedPassMeshDrawList &CachedDrawLists = Scene.CachedDrawLists[CurrMeshPass];
    CommandInfo.CommandIndex = CachedDrawLists.MeshDrawCommands.size();
    CachedDrawLists.MeshDrawCommands.push_back(MeshDrawCommand);
}

MeshPassProcessor::MeshPassProcessor(EMeshPass::Type InMeshPassType, const Scene *InScene)
    : MeshPassType(InMeshPassType), scene(InScene) {}

#define TEMP
#ifdef TEMP
#include "RHI/RHICommandList.h"
#include "RHI/RHIContext.h"
#include "stb_image.h"
struct PerCamera
{
    Mat4 model;
    Mat4 view;
    Mat4 proj;
};
std::shared_ptr<SamplerState> sampler;
std::shared_ptr<Texture> depth;
std::shared_ptr<Texture> tex;
std::shared_ptr<UniformBuffer> ub;
void fuck()
{
    RHICommandListImmediate &dummy = RHICommandListExecutor::GetImmediateCommandList();
    CommandContext *context = GetDefaultContext();
    PerCamera perCamera;
    perCamera.model = Rotate(Mat4(1), Radians(90), Vec3(0, 0, 1));
    perCamera.view = Mat4(1);
    perCamera.proj = Mat4(1);
    UniformBufferLayoutInitializer UBInit;
    UBInit.BindingFlags = UniformBufferBindingFlags::Shader;
    UBInit.ConstantBufferSize = sizeof(PerCamera);
    UBInit.Resources.push_back({offsetof(PerCamera, model), UniformBufferBaseType::UBMT_FLOAT32});
    UBInit.Resources.push_back({offsetof(PerCamera, view), UniformBufferBaseType::UBMT_FLOAT32});
    UBInit.Resources.push_back({offsetof(PerCamera, proj), UniformBufferBaseType::UBMT_FLOAT32});
    UBInit.ComputeHash();
    auto UBLayout = std::make_shared<const UniformBufferLayout>(UBInit);
    ub = CreateUniformBuffer(0, UBLayout, UniformBufferUsage::UniformBuffer_MultiFrame, UniformBufferValidation::None);
    rhi->UpdateUniformBuffer(dummy, ub.get(), &perCamera);
    context->SubmitCommandsHint();
    // 纹理
    int x, y, comp;
    stbi_uc *sourceData = stbi_load("assets/viking_room.png", &x, &y, &comp, STBI_rgb_alpha);
    for (int i = 0; i < x * y; ++i)
    {
        std::swap(sourceData[i * STBI_rgb_alpha + 0], sourceData[i * STBI_rgb_alpha + 2]);
    }
    TextureCreateDesc texDesc = TextureCreateDesc::Create2D("BaseColor", x, y, PF_B8G8R8A8)
                                    .SetInitialState(Access::SRVGraphics)
                                    .SetFlags(TexCreate_SRGB | TexCreate_ShaderResource);
    tex = CreateTexture(dummy, texDesc);
    UpdateTextureRegion2D region(0, 0, 0, 0, x, y);
    UpdateTexture2D(dummy, tex.get(), 0, region, x * STBI_rgb_alpha, sourceData);
    stbi_image_free(sourceData);
    SamplerStateInitializer samplerInit(SF_Point);
    sampler = CreateSamplerState(samplerInit);

    // 深度图
    texDesc.SetFormat(PF_D24)
        .SetInitialState(Access::DSVRead | Access::DSVWrite)
        .SetFlags(TexCreate_DepthStencilTargetable)
        .SetExtent(1600, 1200)
        .SetClearValue(ClearValueBinding(0, 0));
    depth = CreateTexture(dummy, texDesc);
}

void MeshPassProcessor::BuildMeshDrawCommands(const MeshBatch &meshBatch, const MeshPassProcessorRenderState &DrawRenderState,
                                              const MeshProcessorShaders &shaders, RasterizerFillMode MeshFillMode, RasterizerCullMode MeshCullMode)
{
    MeshDrawCommand SharedMeshDrawCommand;

    SharedMeshDrawCommand.StencilRef = 0;

    GraphicsPipelineStateInitializer PipelineState;
    PipelineState.PrimitiveType = (PrimitiveType)meshBatch.Type;

    PipelineState.BoundShaderState.VertexDeclarationRHI = (meshBatch.vertexFactory->GetDeclaration().get());
    PipelineState.BoundShaderState.VertexShaderRHI = shaders.vertexShader;
    PipelineState.BoundShaderState.PixelShaderRHI = shaders.pixelShader;

    PipelineState.RasterizerState = GetStaticRasterizerState<true>(MeshFillMode, MeshCullMode);
    PipelineState.BlendState = DrawRenderState.GetBlendState();
    PipelineState.DepthStencilState = DrawRenderState.GetDepthStencilState();

    for (int i = 0; i < meshBatch.elements.size(); ++i)
    {
        const MeshBatchElement &BatchElement = meshBatch.elements[i];
        MeshDrawCommand &meshDrawCommand = DrawListContext->AddCommand(SharedMeshDrawCommand, meshBatch.elements.size());

        fuck();
        meshDrawCommand.ShaderBindings.uniformBuffers[SF_Vertex].push_back(ub.get());
        meshDrawCommand.ShaderBindings.textureBindings[SF_Pixel].push_back(tex.get());
        meshDrawCommand.ShaderBindings.samplerBindings[SF_Pixel].push_back(sampler.get());

        DrawListContext->FinalizeCommand(meshBatch, i, MeshFillMode, MeshCullMode, PipelineState, meshDrawCommand);
    }
}

#endif

MeshPassProcessor *PassProcessorManager::CreateMeshPassProcessor(EMeshPass::Type PassType, const Scene *Scene)
{
    if (PassType == EMeshPass::Forward)
    {
        return new ForwardPassMeshProcessor(PassType, Scene);
    }
    return nullptr;
}
