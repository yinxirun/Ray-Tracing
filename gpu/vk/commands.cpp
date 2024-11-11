#include "context.h"
#include "pending_state.h"
#include "pipeline.h"
#include "platform.h"
#include "gpu/RHI/RHIGlobals.h"

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

    static constexpr EShaderFrequency Frequency = static_cast<EShaderFrequency>(ShaderType::StaticFrequency);
#ifdef PRINT_UNIMPLEMENT
    printf("Have not implement CommandListContext::SetResourcesFromTables %s\n", __FILE__);
#endif

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
