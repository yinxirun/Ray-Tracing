#include "pipeline_state.h"
#include "context.h"
#include "pipeline.h"
#include "pending_state.h"

void CommonPipelineDescriptorState::CreateDescriptorWriteInfos()
{
	printf("Have not implement CommonPipelineDescriptorState::CreateDescriptorWriteInfos %s %d\n", __FILE__, __LINE__);
}

void CommandListContext::SetGraphicsPipelineState(GraphicsPipelineState *GraphicsState, uint32 StencilRef, bool bApplyAdditionalState)
{
	VulkanGraphicsPipelineState *Pipeline = static_cast<VulkanGraphicsPipelineState *>(GraphicsState);

	PipelineStateCacheManager *PipelineStateCache = device->GetPipelineStateCache();
	PipelineStateCache->LRUTouch(Pipeline);

	CmdBuffer *CmdBuffer = commandBufferManager->GetActiveCmdBuffer();
	bool bForceResetPipeline = !CmdBuffer->bHasPipeline;

	if (pendingGfxState->SetGfxPipeline(Pipeline, bForceResetPipeline))
	{
		pendingGfxState->Bind(CmdBuffer->GetHandle());
		CmdBuffer->bHasPipeline = true;
	}

	pendingGfxState->SetStencilRef(StencilRef);

	if (bApplyAdditionalState)
	{
		// 		ApplyStaticUniformBuffers(static_cast<VulkanVertexShader*>(Pipeline->VulkanShaders[ShaderStage::Vertex]));
		// #if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		// 		ApplyStaticUniformBuffers(static_cast<FVulkanGeometryShader*>(Pipeline->VulkanShaders[ShaderStage::Geometry]));
		// #endif
		// 		ApplyStaticUniformBuffers(static_cast<VulkanPixelShader*>(Pipeline->VulkanShaders[ShaderStage::Pixel]));
	}
}

GraphicsPipelineDescriptorState::GraphicsPipelineDescriptorState(Device *InDevice, VulkanGraphicsPipelineState *InGfxPipeline)
	: CommonPipelineDescriptorState(InDevice), GfxPipeline(InGfxPipeline)
{
	memset(PackedUniformBuffersMask.data(), 0, sizeof(uint64) * ShaderStage::NumStages);
	memset(PackedUniformBuffersDirty.data(), 0, sizeof(uint64) * ShaderStage::NumStages);

	check(InGfxPipeline);
	{
		check(InGfxPipeline->Layout);
		descriptorSetsLayout = &InGfxPipeline->Layout->GetDescriptorSetsLayout();
		VulkanGfxLayout &GfxLayout = *(VulkanGfxLayout *)InGfxPipeline->Layout;
		PipelineDescriptorInfo = &GfxLayout.GetGfxPipelineDescriptorInfo();

		UsedSetsMask = PipelineDescriptorInfo->HasDescriptorsInSetMask;
		const VulkanShaderFactory &ShaderFactory = device->GetShaderFactory();

		const VulkanVertexShader *VertexShader = ShaderFactory.LookupShader<VulkanVertexShader>(InGfxPipeline->GetShaderKey(SF_Vertex));
		check(VertexShader);
		PackedUniformBuffers[ShaderStage::Vertex].Init(VertexShader->GetCodeHeader(), PackedUniformBuffersMask[ShaderStage::Vertex]);

		uint64 PixelShaderKey = InGfxPipeline->GetShaderKey(SF_Pixel);
		if (PixelShaderKey)
		{
			const VulkanPixelShader *PixelShader = ShaderFactory.LookupShader<VulkanPixelShader>(PixelShaderKey);
			check(PixelShader);

			PackedUniformBuffers[ShaderStage::Pixel].Init(PixelShader->GetCodeHeader(), PackedUniformBuffersMask[ShaderStage::Pixel]);
		}

#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		uint64 GeometryShaderKey = InGfxPipeline->GetShaderKey(SF_Geometry);
		if (GeometryShaderKey)
		{
			const FVulkanGeometryShader *GeometryShader = ShaderFactory.LookupShader<FVulkanGeometryShader>(GeometryShaderKey);
			check(GeometryShader);

			PackedUniformBuffers[ShaderStage::Geometry].Init(GeometryShader->GetCodeHeader(), PackedUniformBuffersMask[ShaderStage::Geometry]);
		}
#endif

		CreateDescriptorWriteInfos();
		InGfxPipeline->AddRef();
	}
}