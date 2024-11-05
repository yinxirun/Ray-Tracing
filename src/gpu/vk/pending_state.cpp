#include "pending_state.h"

PendingGfxState::~PendingGfxState()
{
	// TMap<FVulkanRHIGraphicsPipelineState*, FVulkanGraphicsPipelineDescriptorState*> Temp;
	// Swap(Temp, PipelineStates);
	// for (auto& Pair : Temp)
	// {
	// 	FVulkanGraphicsPipelineDescriptorState* State = Pair.Value;
	// 	delete State;
	// }
}