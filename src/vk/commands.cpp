#include "core/assertion_macros.h"
#include "RHI/RHIGlobals.h"
#include "RHI/RHIPipeline.h"
#include "context.h"
#include "pending_state.h"
#include "pipeline.h"
#include "pipeline_state.h"
#include "platform.h"
#include "rhi.h"

static __forceinline ShaderStage::Stage GetAndVerifyShaderStage(GraphicsShader *ShaderRHI, PendingGfxState *PendingGfxState)
{
    switch (ShaderRHI->GetFrequency())
    {
    case SF_Vertex:
        check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Vertex) == GetShaderKey<VulkanVertexShader>(ShaderRHI));
        return ShaderStage::Vertex;
    case SF_Geometry:
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
        check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Geometry) == GetShaderKey<FVulkanGeometryShader>(ShaderRHI));
        return ShaderStage::Geometry;
#else
        checkf(0, "Geometry shaders not supported on this platform!");
        break;
#endif
    case SF_Pixel:
        check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Pixel) == GetShaderKey<VulkanPixelShader>(ShaderRHI));
        return ShaderStage::Pixel;
    default:
        checkf(0, "Undefined FRHIShader Type %d!", (int32)ShaderRHI->GetFrequency());
        break;
    }

    return ShaderStage::Invalid;
}

static __forceinline ShaderStage::Stage GetAndVerifyShaderStageAndVulkanShader(
    GraphicsShader *ShaderRHI, PendingGfxState *PendingGfxState, VulkanShader *&OutShader)
{
    switch (ShaderRHI->GetFrequency())
    {
    case SF_Vertex:
        check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Vertex) == GetShaderKey<VulkanVertexShader>(ShaderRHI));
        OutShader = static_cast<VulkanVertexShader *>(static_cast<VertexShader *>(ShaderRHI));
        return ShaderStage::Vertex;
    case SF_Geometry:
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
        // check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Geometry) ==
        // GetShaderKey<FVulkanGeometryShader>(ShaderRHI));
        OutShader = static_cast<FVulkanGeometryShader *>(
            static_cast<FRHIGeometryShader *>(ShaderRHI));
        return ShaderStage::Geometry;
#else
        check(0);
        break;
#endif
    case SF_Pixel:
        check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Pixel) ==
              GetShaderKey<VulkanPixelShader>(ShaderRHI));
        OutShader = static_cast<VulkanPixelShader *>(static_cast<PixelShader *>(ShaderRHI));
        return ShaderStage::Pixel;
    default:
        check(0);
        break;
    }

    OutShader = nullptr;
    return ShaderStage::Invalid;
}

void CommandListContext::WriteGPUFence(GPUFence *FenceRHI)
{
    CmdBuffer *CmdBuffer = commandBufferManager->GetActiveCmdBuffer();
    VulkanGPUFence *Fence = dynamic_cast<VulkanGPUFence *>(FenceRHI);

    Fence->CmdBuffer = CmdBuffer;
    Fence->FenceSignaledCounter = CmdBuffer->GetFenceSignaledCounter();
}

// 113
void CommandListContext::SetStreamSource(uint32 StreamIndex,
                                         Buffer *VertexBuffer, uint32 Offset)
{
    VulkanMultiBuffer *vertexBuffer =
        static_cast<VulkanMultiBuffer *>(VertexBuffer);
    if (VertexBuffer != nullptr)
    {
        pendingGfxState->SetStreamSource(StreamIndex, vertexBuffer->GetHandle(),
                                         Offset + vertexBuffer->GetOffset());
    }
}

template <class PendingStateType>
struct VulkanResourceBinder
{
    CommandListContext &Context;
    const ShaderFrequency Frequency;
    const std::vector<DescriptorSetRemappingInfo::RemappingInfo> &GlobalRemappingInfo;
    PendingStateType *PendingState;

    VulkanResourceBinder(CommandListContext &InContext, ShaderFrequency InFrequency,
                         const std::vector<DescriptorSetRemappingInfo::RemappingInfo> &InGlobalRemappingInfo,
                         PendingStateType *InPendingState)
        : Context(InContext), Frequency(InFrequency), GlobalRemappingInfo(InGlobalRemappingInfo), PendingState(InPendingState)
    {
    }
};

// 168
template <class ShaderType>
void CommandListContext::SetResourcesFromTables(const ShaderType *Shader)
{
    check(Shader);

    static constexpr ShaderFrequency Frequency = static_cast<ShaderFrequency>(ShaderType::StaticFrequency);

    if (Frequency == SF_Compute)
    {
        printf(
            "Have not implement CommandListContext::SetResourcesFromTables %s %d\n",
            __FILE__, __LINE__);
        // const TArray<FDescriptorSetRemappingInfo::FRemappingInfo>
        // &GlobalRemappingInfo =
        //     PendingComputeState->CurrentState->GetComputePipelineDescriptorInfo().GetGlobalRemappingInfo();

        // FVulkanResourceBinder Binder(*this, Frequency, GlobalRemappingInfo,
        // PendingComputeState); UE::RHICore::SetResourcesFromTables(
        //     Binder, *Shader, Shader->ShaderResourceTable,
        //     DirtyUniformBuffers[Frequency], BoundUniformBuffers[Frequency]);
    }
    else
    {
        const std::vector<DescriptorSetRemappingInfo::RemappingInfo> &GlobalRemappingInfo =
            pendingGfxState->CurrentState->GetGfxPipelineDescriptorInfo().GetGlobalRemappingInfo(ShaderStage::GetStageForFrequency(Frequency));

        VulkanResourceBinder Binder(*this, Frequency, GlobalRemappingInfo, pendingGfxState);
        RHICore::SetResourcesFromTables(
            Binder, *Shader, Shader->ShaderResourceTable,
            DirtyUniformBuffers[Frequency], BoundUniformBuffers[Frequency]);
    }
}

// 210
void CommandListContext::CommitGraphicsResourceTables()
{
    check(pendingGfxState);

    if (const VulkanShader *Shader = pendingGfxState->GetCurrentShader(SF_Vertex))
    {
        check(Shader->Frequency == SF_Vertex);
        const VulkanVertexShader *VertexShader = static_cast<const VulkanVertexShader *>(Shader);
        SetResourcesFromTables(VertexShader);
    }

    if (const VulkanShader *Shader = pendingGfxState->GetCurrentShader(SF_Pixel))
    {
        check(Shader->Frequency == SF_Pixel);
        const VulkanPixelShader *PixelShader = static_cast<const VulkanPixelShader *>(Shader);
        SetResourcesFromTables(PixelShader);
    }

#if PLATFORM_SUPPORTS_MESH_SHADERS
    // :todo-jn: mesh shaders
    // if (const FVulkanShader* Shader =
    // PendingGfxState->GetCurrentShader(SF_Mesh))
    //{
    //  checkSlow(Shader->Frequency == SF_Mesh);
    //	const FVulkanMeshShader* MeshShader = static_cast<const
    // FVulkanMeshShader*>(Shader); 	SetResourcesFromTables(MeshShader);
    //}
    // if (const FVulkanShader* Shader =
    // PendingGfxState->GetCurrentShader(SF_Amplification))
    //{
    //  checkSlow(Shader->Frequency == SF_Amplification);
    //	const FVulkanAmplificationShader* AmplificationShader =
    // static_cast<const FVulkanAmplificationShader*>(Shader);
    //	SetResourcesFromTables(AmplificationShader);
    //}
#endif

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    if (const FVulkanShader *Shader =
            PendingGfxState->GetCurrentShader(SF_Geometry))
    {
        checkSlow(Shader->Frequency == SF_Geometry);
        const FVulkanGeometryShader *GeometryShader =
            static_cast<const FVulkanGeometryShader *>(Shader);
        SetResourcesFromTables(GeometryShader);
    }
#endif
}

void CommandListContext::CommitComputeResourceTables()
{
    // todo
}

void CommandListContext::DispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
    CommitComputeResourceTables();

    CmdBuffer *Cmd = commandBufferManager->GetActiveCmdBuffer();
    ensure(Cmd->IsOutsideRenderPass());
    VkCommandBuffer CmdBuffer = Cmd->GetHandle();
    pendingComputeState->PrepareForDispatch(Cmd);

    vkCmdDispatch(CmdBuffer, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void CommandListContext::SetUAVParameter(ComputeShader *ComputeShaderRHI, uint32 UAVIndex, UnorderedAccessView *UAVRHI)
{
    if (UAVRHI)
    {
        check(pendingComputeState->GetCurrentShader() == static_cast<VulkanComputeShader *>(ComputeShaderRHI));

        VulkanUnorderedAccessView *UAV = static_cast<VulkanUnorderedAccessView *>(UAVRHI);
        pendingComputeState->SetUAVForStage(UAVIndex, UAV);
    }
}

void CommandListContext::SetShaderSampler(GraphicsShader *ShaderRHI, uint32 SamplerIndex, SamplerState *NewStateRHI)
{
    ShaderStage::Stage Stage = GetAndVerifyShaderStage(ShaderRHI, pendingGfxState);
    VulkanSamplerState *Sampler = static_cast<VulkanSamplerState *>(NewStateRHI);
    pendingGfxState->SetSamplerStateForStage(Stage, SamplerIndex, Sampler);
}

void CommandListContext::SetShaderTexture(GraphicsShader *ShaderRHI, uint32 TextureIndex, Texture *NewTextureRHI)
{
    VulkanTexture *vulkanTexture = static_cast<VulkanTexture *>(NewTextureRHI);
    const VkImageLayout ExpectedLayout = LayoutManager::GetDefaultLayout(GetCommandBufferManager()->GetActiveCmdBuffer(),
                                                                         *vulkanTexture, Access::SRVGraphics);

    ShaderStage::Stage Stage = GetAndVerifyShaderStage(ShaderRHI, pendingGfxState);
    pendingGfxState->SetTextureForStage(Stage, TextureIndex, vulkanTexture, ExpectedLayout);
}

void CommandListContext::SetShaderUniformBuffer(GraphicsShader *ShaderRHI, uint32 BufferIndex, UniformBuffer *BufferRHI)
{
    VulkanShader *Shader = nullptr;
    const ShaderStage::Stage Stage = GetAndVerifyShaderStageAndVulkanShader(ShaderRHI, pendingGfxState, Shader);
    check(Shader->GetShaderKey() == pendingGfxState->GetCurrentShaderKey(Stage));

    VulkanUniformBuffer *UniformBuffer = static_cast<VulkanUniformBuffer *>(BufferRHI);
    const ShaderHeader &CodeHeader = Shader->GetCodeHeader();
    const ShaderHeader::UniformBufferInfo &HeaderUBInfo = CodeHeader.UniformBuffers[BufferIndex];
    check(!HeaderUBInfo.LayoutHash || HeaderUBInfo.LayoutHash == UniformBuffer->GetLayout().GetHash());
    const VulkanGfxPipelineDescriptorInfo &DescriptorInfo = pendingGfxState->CurrentState->GetGfxPipelineDescriptorInfo();

    if (!HeaderUBInfo.bOnlyHasResources)
    {
        check(UniformBuffer->GetLayout().ConstantBufferSize > 0);

        uint8 DescriptorSet;
        uint32 BindingIndex;
        if (!DescriptorInfo.GetDescriptorSetAndBindingIndex(ShaderHeader::UniformBuffer, Stage, BufferIndex, DescriptorSet, BindingIndex))
        {
            return;
        }

        const VkDescriptorType DescriptorType = DescriptorInfo.GetDescriptorType(DescriptorSet, BindingIndex);

        if (DescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        {
            pendingGfxState->SetUniformBuffer<true>(DescriptorSet, BindingIndex, UniformBuffer);
        }
        else
        {
            check(DescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            pendingGfxState->SetUniformBuffer<false>(DescriptorSet, BindingIndex, UniformBuffer);
        }
    }

    if (HeaderUBInfo.ResourceEntries.size())
    {
        check(Shader->Frequency < SF_NumStandardFrequencies);
        check(BufferIndex < MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE);
        BoundUniformBuffers[Shader->Frequency][BufferIndex] = UniformBuffer;
        DirtyUniformBuffers[Shader->Frequency] |= (1 << BufferIndex);
    }
    else
    {
        // Internal error: Completely empty UB!
        check(!HeaderUBInfo.bOnlyHasResources);
    }
}

// 661
void CommandListContext::DrawIndexedPrimitive(
    Buffer *IndexBufferRHI, int32 BaseVertexIndex, uint32 FirstInstance,
    uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives,
    uint32 NumInstances)
{
    NumInstances = std::max(1U, NumInstances);

    //	checkf(GRHISupportsFirstInstance || FirstInstance == 0,
    // TEXT("FirstInstance must be 0, see GRHISupportsFirstInstance"));
    check(GRHIGlobals.SupportsFirstInstance || FirstInstance == 0);
    CommitGraphicsResourceTables();

    VulkanMultiBuffer *IndexBuffer = static_cast<VulkanMultiBuffer *>(IndexBufferRHI);
    CmdBuffer *Cmd = commandBufferManager->GetActiveCmdBuffer();
    VkCommandBuffer CmdBuffer = Cmd->GetHandle();
    pendingGfxState->PrepareForDraw(Cmd);

    vkCmdBindIndexBuffer(CmdBuffer, IndexBuffer->GetHandle(),
                         IndexBuffer->GetOffset(), IndexBuffer->GetIndexType());
    uint32 NumIndices = GetVertexCountForPrimitiveCount(
        NumPrimitives, pendingGfxState->primitiveType);

    vkCmdDrawIndexed(CmdBuffer, NumIndices, NumInstances, StartIndex,
                     BaseVertexIndex, FirstInstance);

    if (Platform::RegisterGPUWork() && IsImmediate())
    {
        // 		GpuProfiler.RegisterGPUWork(NumPrimitives * NumInstances,
        // NumVertices * NumInstances);
    }
}

// 877
void CommandListContext::SubmitCommandsHint()
{
    RequestSubmitCurrentCommands();
    CmdBuffer *cmdBuffer = commandBufferManager->GetActiveCmdBuffer();
    if (cmdBuffer && cmdBuffer->HasBegun() && cmdBuffer->IsOutsideRenderPass())
    {
        SafePointSubmit();
    }
    commandBufferManager->RefreshFenceStatus();
}

ComputeContext *RHI::GetCommandContext(RHIPipeline Pipeline, RHIGPUMask GPUMask)
{
    // @todo: RHI command list refactor - fix async compute
    checkf(Pipeline == RHIPipeline::Graphics, "Async compute command contexts not currently implemented.");

    CommandListContext *CmdContext = device->AcquireDeferredContext();

    CommandBufferManager *CmdMgr = CmdContext->GetCommandBufferManager();
    CmdBuffer *cmdBuffer = CmdMgr->GetActiveCmdBuffer();
    if (!cmdBuffer)
    {
        CmdMgr->PrepareForNewActiveCommandBuffer();
        cmdBuffer = CmdMgr->GetActiveCmdBuffer();
    }
    else if (cmdBuffer->IsSubmitted())
    {
        CmdMgr->PrepareForNewActiveCommandBuffer();
        cmdBuffer = CmdMgr->GetActiveCmdBuffer();
    }
    if (!cmdBuffer->HasBegun())
    {
        cmdBuffer->Begin();
    }

    return CmdContext;
}
