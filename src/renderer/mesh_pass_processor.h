#pragma once

#include "definitions.h"
#include "render_core/vertex_factory.h"
#include "RHI/dynamic_rhi.h"
#include <vector>
#include <array>

class Scene;
class MeshBatch;
class PrimitiveComponent;

struct MeshProcessorShaders
{
    VertexShader *vertexShader;
    PixelShader *pixelShader;
};

/** Mesh pass types supported. */
namespace EMeshPass
{
    enum Type : uint8
    {
        Forward,
        Num,
        NumBits = 6,
    };
}

/** Mesh pass mask - stores one bit per mesh pass. */
class MeshPassMask
{
public:
    MeshPassMask() : Data(0) {}

    void Set(EMeshPass::Type Pass)
    {
        Data |= (uint64(1) << Pass);
    }

    bool Get(EMeshPass::Type Pass) const
    {
        return !!(Data & (uint64(1) << Pass));
    }

    uint64 Data;
};

/// Encapsulates shader bindings for a single FMeshDrawCommand.
class MeshDrawShaderBindings
{
public:
    std::array<std::vector<UniformBuffer *>, SF_NumFrequencies> uniformBuffers;
    std::array<std::vector<Texture *>, SF_NumFrequencies> textureBindings;
    std::array<std::vector<SamplerState *>, SF_NumFrequencies> samplerBindings;
};

class MeshPassProcessorRenderState
{
public:
    MeshPassProcessorRenderState() = default;
    ~MeshPassProcessorRenderState() = default;

    inline BlendState *GetBlendState() const { return BlendState; }

    inline DepthStencilState *GetDepthStencilState() const { return DepthStencilState; }

private:
    BlendState *BlendState = nullptr;
    DepthStencilState *DepthStencilState = nullptr;
    ExclusiveDepthStencil::Type DepthStencilAccess = ExclusiveDepthStencil::DepthRead_StencilRead;

    UniformBuffer *ViewUniformBuffer = nullptr;

    uint32 StencilRef = 0;
};

/**
 * FMeshDrawCommand fully describes a mesh pass draw call, captured just above the RHI.
        FMeshDrawCommand should contain only data needed to draw.  For InitViews payloads, use FVisibleMeshDrawCommand.
 * FMeshDrawCommands are cached at Primitive AddToScene time for vertex factories that support it (no per-frame or per-view shader binding changes).
 * Dynamic Instancing operates at the FMeshDrawCommand level for robustness.
        Adding per-command shader bindings will reduce the efficiency of Dynamic Instancing, but rendering will always be correct.
 * Any resources referenced by a command must be kept alive for the lifetime of the command.  FMeshDrawCommand is not responsible for lifetime management of resources.
        For uniform buffers referenced by cached FMeshDrawCommand's, RHIUpdateUniformBuffer makes it possible to access per-frame data in the shader without changing bindings.
 */
class MeshDrawCommand
{
public:
    // Resource bindings
    MeshDrawShaderBindings ShaderBindings;
    std::vector<VertexInputStream> VertexStreams;
    Buffer *IndexBuffer;

    // PSO
    GraphicsPipelineStateInitializer PsoInit;

    // Draw command parameters
    uint32 FirstIndex;
    uint32 NumPrimitives;

    union
    {
        struct
        {
            uint32 BaseVertexIndex;
            uint32 NumVertices;
        } VertexParams;

        struct
        {
            Buffer *Buffer;
            uint32 Offset;
        } IndirectArgs;
    };

    /** Non-pipeline state */
    uint8 StencilRef;

    /** Called when the mesh draw command is complete. */
    void SetDrawParametersAndFinalize(
        const MeshBatch &MeshBatch,
        int32 BatchElementIndex,
        GraphicsPipelineStateInitializer PipelineId);
};

// Stores information about a mesh draw command which is cached in the scene.
// This is stored separately from the cached FMeshDrawCommand so that InitViews does not have to load the FMeshDrawCommand into cache.
class CachedMeshDrawCommandInfo
{
public:
    CachedMeshDrawCommandInfo() : CachedMeshDrawCommandInfo(EMeshPass::Num)
    {
    }

    explicit CachedMeshDrawCommandInfo(EMeshPass::Type InMeshPass)
        : CommandIndex(-1)
    {
    }
    // Stores the index into FScene::CachedDrawLists of the corresponding FMeshDrawCommand, or -1 if not stored there
    int32 CommandIndex;

    // Stores the index into FScene::CachedMeshDrawCommandStateBuckets of the corresponding FMeshDrawCommand, or -1 if not stored there
    int32 StateBucketId = -1;

    // Needed for view overrides
    RasterizerFillMode MeshFillMode : ERasterizerFillMode_NumBits + 1;
    RasterizerCullMode MeshCullMode : ERasterizerCullMode_NumBits + 1;
};

class CachedPassMeshDrawList
{
public:
    CachedPassMeshDrawList() : LowestFreeIndexSearchStart(0) {}

    /** Indices held by FStaticMeshBatch::CachedMeshDrawCommands must be stable */
    std::vector<MeshDrawCommand> MeshDrawCommands;
    int32 LowestFreeIndexSearchStart;
};

/** Interface for the different types of draw lists. */
class MeshPassDrawListContext
{
public:
    virtual ~MeshPassDrawListContext() {}
    virtual MeshDrawCommand &AddCommand(MeshDrawCommand &Initializer, uint32 NumElements) = 0;
    virtual void FinalizeCommand(const MeshBatch &MeshBatch, int32 BatchElementIndex, RasterizerFillMode MeshFillMode,
                                 RasterizerCullMode MeshCullMode, const GraphicsPipelineStateInitializer &PipelineState,
                                 MeshDrawCommand &MeshDrawCommand) = 0;
};

class CachedPassMeshDrawListContext : public MeshPassDrawListContext
{
public:
    CachedPassMeshDrawListContext(Scene &InScene) : scene(InScene) {}
    CachedMeshDrawCommandInfo GetCommandInfoAndReset();
    virtual MeshDrawCommand &AddCommand(MeshDrawCommand &Initializer, uint32 NumElements) override final;

protected:
    Scene &scene;
    MeshDrawCommand MeshDrawCommandForStateBucketing;
    CachedMeshDrawCommandInfo CommandInfo;
    EMeshPass::Type CurrMeshPass = EMeshPass::Num;
};

class CachedPassMeshDrawListContextImmediate : public CachedPassMeshDrawListContext
{
public:
    CachedPassMeshDrawListContextImmediate(Scene &InScene) : CachedPassMeshDrawListContext(InScene) {};
    virtual void FinalizeCommand(const MeshBatch &MeshBatch, int32 BatchElementIndex, RasterizerFillMode MeshFillMode,
                                 RasterizerCullMode MeshCullMode,
                                 const GraphicsPipelineStateInitializer &PipelineState, MeshDrawCommand &MeshDrawCommand) override final;
};

/**
 * Base class of mesh processors, whose job is to transform FMeshBatch draw descriptions received from scene proxy implementations into FMeshDrawCommands ready for the RHI command list
 */
class MeshPassProcessor
{
public:
    EMeshPass::Type MeshPassType;
    const Scene *scene;
    MeshPassDrawListContext *DrawListContext;
    MeshPassProcessor(EMeshPass::Type InMeshPassType, const Scene *InScene);
    virtual ~MeshPassProcessor() {}

    // FMeshPassProcessor interface
    // Add a FMeshBatch to the pass
    virtual void AddMeshBatch(const MeshBatch &meshBatch, uint64 BatchElementMask,
                              const PrimitiveComponent *PrimitiveSceneProxy, int32 StaticMeshId = -1) = 0;

    void BuildMeshDrawCommands(const MeshBatch &meshBatch, const MeshPassProcessorRenderState &state, const MeshProcessorShaders &shaders,
                               RasterizerFillMode MeshFillMode, RasterizerCullMode MeshCullMode);
};

class PassProcessorManager
{
public:
    static MeshPassProcessor *CreateMeshPassProcessor(EMeshPass::Type PassType, const Scene *Scene,
                                                      MeshPassDrawListContext *InDrawListContext);
};

class VisibleMeshDrawCommand
{
public:
    inline void Setup(const MeshDrawCommand *InMeshDrawCommand)
    {
        meshDrawCommand = InMeshDrawCommand;
    }

    // Mesh Draw Command stored separately to avoid fetching its data during sorting
    const MeshDrawCommand *meshDrawCommand;
};