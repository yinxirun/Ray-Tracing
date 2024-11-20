#include "context.h"
#include "pending_state.h"
#include "pipeline.h"
#include "platform.h"
#include "pipeline_state.h"
#include "gpu/RHI/RHIGlobals.h"

static __forceinline ShaderStage::Stage GetAndVerifyShaderStageAndVulkanShader(RHIGraphicsShader *ShaderRHI, PendingGfxState *PendingGfxState, VulkanShader *&OutShader)
{
    switch (ShaderRHI->GetFrequency())
    {
    case SF_Vertex:
        // check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Vertex) == GetShaderKey<FVulkanVertexShader>(ShaderRHI));
        OutShader = static_cast<VulkanVertexShader *>(static_cast<VertexShader *>(ShaderRHI));
        return ShaderStage::Vertex;
    case SF_Geometry:
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
        // check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Geometry) == GetShaderKey<FVulkanGeometryShader>(ShaderRHI));
        OutShader = static_cast<FVulkanGeometryShader *>(static_cast<FRHIGeometryShader *>(ShaderRHI));
        return ShaderStage::Geometry;
#else
        check(0);
        break;
#endif
    case SF_Pixel:
        // check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Pixel) == GetShaderKey<FVulkanPixelShader>(ShaderRHI));
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
void CommandListContext::SetStreamSource(uint32 StreamIndex, Buffer *VertexBuffer, uint32 Offset)
{
    VulkanMultiBuffer *vertexBuffer = static_cast<VulkanMultiBuffer *>(VertexBuffer);
    if (VertexBuffer != nullptr)
    {
        pendingGfxState->SetStreamSource(StreamIndex, vertexBuffer->GetHandle(), Offset + vertexBuffer->GetOffset());
    }
}

// 168
template <class ShaderType>
void CommandListContext::SetResourcesFromTables(const ShaderType *Shader)
{
    check(Shader);

    static constexpr ShaderFrequency Frequency = static_cast<ShaderFrequency>(ShaderType::StaticFrequency);
    printf("Have not implement CommandListContext::SetResourcesFromTables %s %d\n", __FILE__, __LINE__);

    if (Frequency == SF_Compute)
    {
        // const TArray<FDescriptorSetRemappingInfo::FRemappingInfo> &GlobalRemappingInfo =
        //     PendingComputeState->CurrentState->GetComputePipelineDescriptorInfo().GetGlobalRemappingInfo();

        // FVulkanResourceBinder Binder(*this, Frequency, GlobalRemappingInfo, PendingComputeState);
        // UE::RHICore::SetResourcesFromTables(
        //     Binder, *Shader, Shader->ShaderResourceTable, DirtyUniformBuffers[Frequency], BoundUniformBuffers[Frequency]);
    }
    else
    {
        // const TArray<FDescriptorSetRemappingInfo::FRemappingInfo> &GlobalRemappingInfo =
        //     PendingGfxState->CurrentState->GetGfxPipelineDescriptorInfo().GetGlobalRemappingInfo(ShaderStage::GetStageForFrequency(Frequency));

        // FVulkanResourceBinder Binder(*this, Frequency, GlobalRemappingInfo, PendingGfxState);
        // UE::RHICore::SetResourcesFromTables(
        //     Binder, *Shader, Shader->ShaderResourceTable, DirtyUniformBuffers[Frequency], BoundUniformBuffers[Frequency]);
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
    // if (const FVulkanShader* Shader = PendingGfxState->GetCurrentShader(SF_Mesh))
    //{
    //  checkSlow(Shader->Frequency == SF_Mesh);
    //	const FVulkanMeshShader* MeshShader = static_cast<const FVulkanMeshShader*>(Shader);
    //	SetResourcesFromTables(MeshShader);
    //}
    // if (const FVulkanShader* Shader = PendingGfxState->GetCurrentShader(SF_Amplification))
    //{
    //  checkSlow(Shader->Frequency == SF_Amplification);
    //	const FVulkanAmplificationShader* AmplificationShader = static_cast<const FVulkanAmplificationShader*>(Shader);
    //	SetResourcesFromTables(AmplificationShader);
    //}
#endif

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    if (const FVulkanShader *Shader = PendingGfxState->GetCurrentShader(SF_Geometry))
    {
        checkSlow(Shader->Frequency == SF_Geometry);
        const FVulkanGeometryShader *GeometryShader = static_cast<const FVulkanGeometryShader *>(Shader);
        SetResourcesFromTables(GeometryShader);
    }
#endif
}

inline void CommandListContext::SetShaderUniformBuffer(ShaderStage::Stage Stage, const VulkanUniformBuffer *UniformBuffer, int32 BufferIndex, const VulkanShader *Shader)
{
    check(Shader->GetShaderKey() == pendingGfxState->GetCurrentShaderKey(Stage));

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
            check(DescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && false);
            /* pendingGfxState->SetUniformBuffer<false>(DescriptorSet, BindingIndex, UniformBuffer); */
        }
    }

    if (HeaderUBInfo.ResourceEntries.size())
    {
        SetShaderUniformBufferResources(this, pendingGfxState, Shader, CodeHeader.Globals, CodeHeader.GlobalDescriptorTypes, HeaderUBInfo, UniformBuffer, DescriptorInfo.GetGlobalRemappingInfo(Stage));
    }
    else
    {
        // Internal error: Completely empty UB!
        check(!HeaderUBInfo.bOnlyHasResources);
    }
}

void CommandListContext::SetShaderUniformBuffer(RHIGraphicsShader *ShaderRHI, uint32 BufferIndex, UniformBuffer *BufferRHI)
{
    VulkanShader *shader = nullptr;
    ShaderStage::Stage stage = GetAndVerifyShaderStageAndVulkanShader(ShaderRHI, pendingGfxState, shader);
    VulkanUniformBuffer *UniformBuffer = static_cast<VulkanUniformBuffer *>(BufferRHI);
    SetShaderUniformBuffer(stage, UniformBuffer, BufferIndex, shader);
}

// 661
void CommandListContext::DrawIndexedPrimitive(Buffer *IndexBufferRHI, int32 BaseVertexIndex, uint32 FirstInstance,
                                              uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
    NumInstances = std::max(1U, NumInstances);

    //	checkf(GRHISupportsFirstInstance || FirstInstance == 0, TEXT("FirstInstance must be 0, see GRHISupportsFirstInstance"));
    check(GRHIGlobals.SupportsFirstInstance || FirstInstance == 0);
    CommitGraphicsResourceTables();

    VulkanMultiBuffer *IndexBuffer = static_cast<VulkanMultiBuffer *>(IndexBufferRHI);
    CmdBuffer *Cmd = commandBufferManager->GetActiveCmdBuffer();
    VkCommandBuffer CmdBuffer = Cmd->GetHandle();
    pendingGfxState->PrepareForDraw(Cmd);

    vkCmdBindIndexBuffer(CmdBuffer, IndexBuffer->GetHandle(), IndexBuffer->GetOffset(), IndexBuffer->GetIndexType());
    uint32 NumIndices = GetVertexCountForPrimitiveCount(NumPrimitives, pendingGfxState->primitiveType);

    vkCmdDrawIndexed(CmdBuffer, NumIndices, NumInstances, StartIndex, BaseVertexIndex, FirstInstance);

    if (Platform::RegisterGPUWork() && IsImmediate())
    {
        // 		GpuProfiler.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
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
