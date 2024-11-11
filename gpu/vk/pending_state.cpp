#include "pending_state.h"
#include "command_buffer.h"
#include "pipeline_state.h"
#include "private.h"
#include "pipeline.h"
#include "context.h"

PendingGfxState::~PendingGfxState()
{
	std::unordered_map<VulkanGraphicsPipelineState *, GraphicsPipelineDescriptorState *> Temp;
	std::swap(Temp, PipelineStates);
	for (auto &Pair : Temp)
	{
		GraphicsPipelineDescriptorState *State = Pair.second;
		delete State;
	}
}

void PendingGfxState::PrepareForDraw(CmdBuffer *CmdBuffer)
{
	check(CmdBuffer->bHasPipeline);

	if (device->SupportsBindless())
	{
		printf("Error: Don't support Bindless %s\n", __FILE__);
		exit(-1);
		// check(!CurrentPipeline->bHasInputAttachments); // todo-jn: bindless + InputAttachments
		// UpdateDynamicStates(CmdBuffer);
		// CurrentState->UpdateBindlessDescriptors(&Context, CmdBuffer);
	}
	else
	{
		// TODO: Add 'dirty' flag? Need to rebind only on PSO change
		if (CurrentPipeline->bHasInputAttachments)
		{
			printf("Error: Don't support InputAttachments %s\n", __FILE__);
			exit(-1);
			Framebuffer *CurrentFramebuffer = context.GetCurrentFramebuffer();
			// UpdateInputAttachments(CurrentFramebuffer);
		}

		const bool bHasDescriptorSets = CurrentState->UpdateDescriptorSets(&context, CmdBuffer);

		UpdateDynamicStates(CmdBuffer);

		if (bHasDescriptorSets)
		{
			printf("Error: Don't support DescriptorSets %s\n", __FILE__);
			exit(-1);
			CurrentState->BindDescriptorSets(CmdBuffer->GetHandle());
		}
	}

	if (bDirtyVertexStreams)
	{
		// Its possible to have no vertex buffers
		const VertexInputStateInfo &VertexInputStateInfo = CurrentPipeline->GetVertexInputState();
		if (VertexInputStateInfo.AttributesNum == 0)
		{
			// However, we need to verify that there are also no bindings
			check(VertexInputStateInfo.BindingsNum == 0);
			return;
		}

		struct FTemporaryIA
		{
			VkBuffer VertexBuffers[MaxVertexElementCount];
			VkDeviceSize VertexOffsets[MaxVertexElementCount];
			int32 NumUsed = 0;

			void Add(VkBuffer InBuffer, VkDeviceSize InSize)
			{
				check(NumUsed < MaxVertexElementCount);
				VertexBuffers[NumUsed] = InBuffer;
				VertexOffsets[NumUsed] = InSize;
				++NumUsed;
			}
		} TemporaryIA;

		const VkVertexInputAttributeDescription *CurrAttribute = nullptr;
		for (uint32 BindingIndex = 0; BindingIndex < VertexInputStateInfo.BindingsNum; BindingIndex++)
		{
			const VkVertexInputBindingDescription &CurrBinding = VertexInputStateInfo.Bindings[BindingIndex];

			auto it = VertexInputStateInfo.BindingToStream.find(BindingIndex);
			check(it != VertexInputStateInfo.BindingToStream.end());
			uint32 StreamIndex = it->second;
			PendingGfxState::VertexStream &CurrStream = PendingStreams[StreamIndex];

			// Verify the vertex buffer is set
			if (CurrStream.Stream == VK_NULL_HANDLE)
			{
				// The attribute in stream index is probably compiled out
#if UE_BUILD_DEBUG
				// Lets verify
				for (uint32 AttributeIndex = 0; AttributeIndex < VertexInputStateInfo.AttributesNum; AttributeIndex++)
				{
					if (VertexInputStateInfo.Attributes[AttributeIndex].binding == CurrBinding.binding)
					{
						uint64 VertexShaderKey = GetCurrentShaderKey(ShaderStage::Vertex);
						FVulkanVertexShader *VertexShader = Device->GetShaderFactory().LookupShader<FVulkanVertexShader>(VertexShaderKey);
						UE_LOG(LogVulkanRHI, Warning, TEXT("Missing input assembly binding on location %d in Vertex shader '%s'"),
							   CurrBinding.binding,
							   VertexShader ? *VertexShader->GetDebugName() : TEXT("Null"));
						ensure(0);
					}
				}
#endif
				continue;
			}

			TemporaryIA.Add(CurrStream.Stream, CurrStream.BufferOffset);
		}

		if (TemporaryIA.NumUsed > 0)
		{
			// Bindings are expected to be in ascending order with no index gaps in between:
			// Correct:		0, 1, 2, 3
			// Incorrect:	1, 0, 2, 3
			// Incorrect:	0, 2, 3, 5
			// Reordering and creation of stream binding index is done in "GenerateVertexInputStateInfo()"
			vkCmdBindVertexBuffers(CmdBuffer->GetHandle(), 0, TemporaryIA.NumUsed, TemporaryIA.VertexBuffers, TemporaryIA.VertexOffsets);
		}

		bDirtyVertexStreams = false;
	}
}

void PendingGfxState::InternalUpdateDynamicStates(CmdBuffer *Cmd)
{
	const bool bNeedsUpdateViewport = !Cmd->bHasViewport ||
									  viewports.size() != Cmd->CurrentViewports.size() ||
									  (memcmp((const void *)Cmd->CurrentViewports.data(),
											  (const void *)viewports.data(),
											  viewports.size() * sizeof(VkViewport)) != 0);
	// Validate and update Viewport
	if (bNeedsUpdateViewport)
	{
		// it is legal to pass a zero-area viewport, and the higher level expectation (see e.g. FProjectedShadowInfo::SetupProjectionStencilMask()) is that
		// such viewport is going to be essentially disabled.

		// Flip viewport on Y-axis to be uniform between DXC generated SPIR-V shaders (requires VK_KHR_maintenance1 extension)
		std::vector<VkViewport> FlippedViewports = viewports;
		for (VkViewport &FlippedViewport : FlippedViewports)
		{
			FlippedViewport.y += FlippedViewport.height;
			FlippedViewport.height = -FlippedViewport.height;
		}
		vkCmdSetViewport(Cmd->GetHandle(), 0, FlippedViewports.size(), FlippedViewports.data());

		Cmd->CurrentViewports = viewports;
		Cmd->bHasViewport = true;
	}

	const bool bNeedsUpdateScissor = !Cmd->bHasScissor ||
									 scissors.size() != Cmd->CurrentScissors.size() ||
									 (memcmp((const void *)Cmd->CurrentScissors.data(),
											 (const void *)scissors.data(),
											 scissors.size() * sizeof(VkRect2D)) != 0);
	if (bNeedsUpdateScissor)
	{
		vkCmdSetScissor(Cmd->GetHandle(), 0, scissors.size(), scissors.data());
		Cmd->CurrentScissors = scissors;
		Cmd->bHasScissor = true;
	}

	const bool bNeedsUpdateStencil = !Cmd->bHasStencilRef || (Cmd->CurrentStencilRef != StencilRef);
	if (bNeedsUpdateStencil)
	{
		vkCmdSetStencilReference(Cmd->GetHandle(), VK_STENCIL_FRONT_AND_BACK, StencilRef);
		Cmd->CurrentStencilRef = StencilRef;
		Cmd->bHasStencilRef = true;
	}

	Cmd->bNeedsDynamicStateSet = false;
}

bool PendingGfxState::SetGfxPipeline(VulkanGraphicsPipelineState *InGfxPipeline, bool bForceReset)
{
	bool bChanged = bForceReset;

	if (InGfxPipeline != CurrentPipeline)
	{
		CurrentPipeline = InGfxPipeline;
		auto it = PipelineStates.find(InGfxPipeline);
		if (it != PipelineStates.end())
		{
			CurrentState = it->second;
			check(CurrentState->GfxPipeline == InGfxPipeline);
		}
		else
		{
			CurrentState = new GraphicsPipelineDescriptorState(device, InGfxPipeline);
			PipelineStates.insert(std::pair(CurrentPipeline, CurrentState));
		}

		primitiveType = InGfxPipeline->PrimitiveType;
		bChanged = true;
	}

	if (bChanged || bForceReset)
	{
		CurrentState->Reset();
	}

	return bChanged;
}