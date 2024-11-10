#include "pipeline_state.h"
#include "context.h"
#include "pipeline.h"
#include "pending_state.h"

void CommandListContext::SetGraphicsPipelineState(GraphicsPipelineState* GraphicsState, uint32 StencilRef, bool bApplyAdditionalState)
{
	VulkanGraphicsPipelineState* Pipeline = static_cast<VulkanGraphicsPipelineState*>(GraphicsState);
	
	PipelineStateCacheManager* PipelineStateCache = device->GetPipelineStateCache();
// 	PipelineStateCache->LRUTouch(Pipeline);

	CmdBuffer* CmdBuffer = commandBufferManager->GetActiveCmdBuffer();
// 	bool bForceResetPipeline = !CmdBuffer->bHasPipeline;

// 	if (pendingGfxState->SetGfxPipeline(Pipeline, bForceResetPipeline))
// 	{
// 		pendingGfxState->Bind(CmdBuffer->GetHandle());
// 		CmdBuffer->bHasPipeline = true;
// 		pendingGfxState->MarkNeedsDynamicStates();
// 	}

// 	pendingGfxState->SetStencilRef(StencilRef);

// 	if (bApplyAdditionalState)
// 	{
// 		ApplyStaticUniformBuffers(static_cast<VulkanVertexShader*>(Pipeline->VulkanShaders[ShaderStage::Vertex]));
// #if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
// 		ApplyStaticUniformBuffers(static_cast<FVulkanGeometryShader*>(Pipeline->VulkanShaders[ShaderStage::Geometry]));
// #endif
// 		ApplyStaticUniformBuffers(static_cast<VulkanPixelShader*>(Pipeline->VulkanShaders[ShaderStage::Pixel]));
// 	}
}