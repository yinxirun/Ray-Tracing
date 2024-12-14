#include "pipeline_state.h"
#include "context.h"
#include "pipeline.h"
#include "pending_state.h"
#include "state.h"

static bool ShouldAlwaysWriteDescriptors() { return false; }

ComputePipelineDescriptorState::ComputePipelineDescriptorState(Device *InDevice, VulkanComputePipeline *InComputePipeline)
	: CommonPipelineDescriptorState(InDevice), PackedUniformBuffersMask(0), PackedUniformBuffersDirty(0), ComputePipeline(InComputePipeline)
{
	check(InComputePipeline);
	const ShaderHeader &CodeHeader = InComputePipeline->GetShaderCodeHeader();
	PackedUniformBuffers.Init(CodeHeader, PackedUniformBuffersMask);

	descriptorSetsLayout = &InComputePipeline->GetLayout().GetDescriptorSetsLayout();
	PipelineDescriptorInfo = &InComputePipeline->GetComputeLayout().GetComputePipelineDescriptorInfo();

	UsedSetsMask = PipelineDescriptorInfo->HasDescriptorsInSetMask;

	CreateDescriptorWriteInfos();
	InComputePipeline->AddRef();

	ensure(DSWriter.size() == 0 || DSWriter.size() == 1);
}

void CommandListContext::SetComputePipelineState(ComputePipelineState *ComputePipelineState)
{
	CmdBuffer *cmdBuffer = commandBufferManager->GetActiveCmdBuffer();

	if (cmdBuffer->CurrentDescriptorPoolSetContainer == nullptr)
	{
		cmdBuffer->CurrentDescriptorPoolSetContainer = &device->GetDescriptorPoolsManager().AcquirePoolSetContainer();
	}

	// #todo-rco: Set PendingGfx to null
	VulkanComputePipeline *ComputePipeline = static_cast<VulkanComputePipeline *>(ComputePipelineState);
	pendingComputeState->SetComputePipeline(ComputePipeline);

	/* ApplyStaticUniformBuffers(const_cast<VulkanComputeShader*>(ComputePipeline->GetShader())); */
}

template <bool bUseDynamicGlobalUBs>
bool ComputePipelineDescriptorState::InternalUpdateDescriptorSets(CommandListContext *CmdListContext, CmdBuffer *CmdBuffer)
{
	// Early exit
	if (!UsedSetsMask)
	{
		return false;
	}

	VulkanUniformBufferUploader *UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8 *CPURingBufferBase = (uint8 *)UniformBufferUploader->GetCPUMappedPointer();
	const VkDeviceSize UBOffsetAlignment = device->GetLimits().minUniformBufferOffsetAlignment;

	if (PackedUniformBuffersDirty != 0)
	{
		check(0);
	}

	// We are not using UseVulkanDescriptorCache() for compute pipelines
	// Compute tend to use volatile resources that polute descriptor cache

	if (!CmdBuffer->AcquirePoolSetAndDescriptorsIfNeeded(*descriptorSetsLayout, true, DescriptorSetHandles.data()))
	{
		return false;
	}

	const VkDescriptorSet DescriptorSet = DescriptorSetHandles[0];
	DSWriter[0].SetDescriptorSet(DescriptorSet);

	{
		vkUpdateDescriptorSets(device->GetInstanceHandle(), DSWriteContainer.DescriptorWrites.size(),
							   DSWriteContainer.DescriptorWrites.data(), 0, nullptr);
	}

	return true;
}

// 694
void DescriptorSetWriter::Reset()
{
	bHasVolatileResources = false;
#if VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
	WrittenMask = BaseWrittenMask;
#endif
}

void DescriptorSetWriter::SetWritten(uint32 DescriptorIndex)
{
#if VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
	uint32 Index = DescriptorIndex / 32;
	uint32 Mask = DescriptorIndex % 32;
	WrittenMask[Index] |= (1 << Mask);
#endif
}

// 710
void DescriptorSetWriter::SetWrittenBase(uint32 DescriptorIndex)
{
#if VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
	uint32 Index = DescriptorIndex / 32;
	uint32 Mask = DescriptorIndex % 32;
	BaseWrittenMask[Index] |= (1 << Mask);
#endif
}

void DescriptorSetWriter::InitWrittenMasks(uint32 NumDescriptorWrites)
{
#if VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
	uint32 Size = (NumDescriptorWrites + 31) / 32;
	WrittenMask.Empty(Size);
	WrittenMask.SetNumZeroed(Size);
	BaseWrittenMask.Empty(Size);
	BaseWrittenMask.SetNumZeroed(Size);
#endif
}

void CommonPipelineDescriptorState::CreateDescriptorWriteInfos()
{
	check(DSWriteContainer.DescriptorWrites.size() == 0);

	const int32 NumSets = descriptorSetsLayout->RemappingInfo.SetInfos.size();
	check(UsedSetsMask <= (uint32)(((uint32)1 << NumSets) - 1));

	for (int32 Set = 0; Set < NumSets; ++Set)
	{
		const DescriptorSetRemappingInfo::SetInfo &SetInfo = descriptorSetsLayout->RemappingInfo.SetInfos[Set];

		if (UseVulkanDescriptorCache())
		{
			DSWriteContainer.HashableDescriptorInfo.resize(DSWriteContainer.HashableDescriptorInfo.size() +
														   SetInfo.Types.size() + 1); // Add 1 for the Layout
		}
		DSWriteContainer.DescriptorWrites.resize(DSWriteContainer.DescriptorWrites.size() + SetInfo.Types.size());
		DSWriteContainer.DescriptorImageInfo.resize(DSWriteContainer.DescriptorImageInfo.size() + SetInfo.NumImageInfos);
		DSWriteContainer.DescriptorBufferInfo.resize(DSWriteContainer.DescriptorBufferInfo.size() + SetInfo.NumBufferInfos);

#if VULKAN_RHI_RAYTRACING
		DSWriteContainer.AccelerationStructureWrites.AddZeroed(SetInfo.NumAccelerationStructures);
		DSWriteContainer.AccelerationStructures.AddZeroed(SetInfo.NumAccelerationStructures);
#endif // VULKAN_RHI_RAYTRACING

		check(SetInfo.Types.size() < 255);
		DSWriteContainer.BindingToDynamicOffsetMap.resize(DSWriteContainer.BindingToDynamicOffsetMap.size() + SetInfo.Types.size());
	}

	memset(DSWriteContainer.BindingToDynamicOffsetMap.data(), 255, DSWriteContainer.BindingToDynamicOffsetMap.size());

	check(DSWriter.size() == 0);
	DSWriter.resize(NumSets);

	const VulkanSamplerState &DefaultSampler = device->GetDefaultSampler();
	const View::TextureView &DefaultImageView = device->GetDefaultImageView();

	HashableDescriptorInfo *CurrentHashableDescriptorInfo = nullptr;
	if (UseVulkanDescriptorCache())
	{
		CurrentHashableDescriptorInfo = DSWriteContainer.HashableDescriptorInfo.data();
	}
	VkWriteDescriptorSet *CurrentDescriptorWrite = DSWriteContainer.DescriptorWrites.data();
	VkDescriptorImageInfo *CurrentImageInfo = DSWriteContainer.DescriptorImageInfo.data();
	VkDescriptorBufferInfo *CurrentBufferInfo = DSWriteContainer.DescriptorBufferInfo.data();

#if VULKAN_RHI_RAYTRACING
	VkWriteDescriptorSetAccelerationStructureKHR *CurrentAccelerationStructuresWriteDescriptors = DSWriteContainer.AccelerationStructureWrites.GetData();
	VkAccelerationStructureKHR *CurrentAccelerationStructures = DSWriteContainer.AccelerationStructures.GetData();
#endif // VULKAN_RHI_RAYTRACING

	uint8 *CurrentBindingToDynamicOffsetMap = DSWriteContainer.BindingToDynamicOffsetMap.data();
	std::vector<uint32> DynamicOffsetsStart;
	DynamicOffsetsStart.resize(NumSets);
	uint32 TotalNumDynamicOffsets = 0;

	for (int32 Set = 0; Set < NumSets; ++Set)
	{
		const DescriptorSetRemappingInfo::SetInfo &SetInfo = descriptorSetsLayout->RemappingInfo.SetInfos[Set];

		DynamicOffsetsStart[Set] = TotalNumDynamicOffsets;

		uint32 NumDynamicOffsets = DSWriter[Set].SetupDescriptorWrites(
			SetInfo.Types, CurrentHashableDescriptorInfo,
			CurrentDescriptorWrite, CurrentImageInfo, CurrentBufferInfo, CurrentBindingToDynamicOffsetMap,
#if VULKAN_RHI_RAYTRACING
			CurrentAccelerationStructuresWriteDescriptors,
			CurrentAccelerationStructures,
#endif // VULKAN_RHI_RAYTRACING
			DefaultSampler, DefaultImageView);

		TotalNumDynamicOffsets += NumDynamicOffsets;

		if (CurrentHashableDescriptorInfo) // UseVulkanDescriptorCache()
		{
			CurrentHashableDescriptorInfo += SetInfo.Types.size();
			CurrentHashableDescriptorInfo->Layout.Max0 = UINT32_MAX;
			CurrentHashableDescriptorInfo->Layout.Max1 = UINT32_MAX;
			CurrentHashableDescriptorInfo->Layout.LayoutId = descriptorSetsLayout->GetHandleIds()[Set];
			++CurrentHashableDescriptorInfo;
		}

		CurrentDescriptorWrite += SetInfo.Types.size();
		CurrentImageInfo += SetInfo.NumImageInfos;
		CurrentBufferInfo += SetInfo.NumBufferInfos;

#if VULKAN_RHI_RAYTRACING
		CurrentAccelerationStructuresWriteDescriptors += SetInfo.NumAccelerationStructures;
		CurrentAccelerationStructures += SetInfo.NumAccelerationStructures;
#endif // VULKAN_RHI_RAYTRACING

		CurrentBindingToDynamicOffsetMap += SetInfo.Types.size();
	}

	DynamicOffsets.resize(TotalNumDynamicOffsets);
	for (int32 Set = 0; Set < NumSets; ++Set)
	{
		DSWriter[Set].DynamicOffsets = DynamicOffsetsStart[Set] + DynamicOffsets.data();
	}

	DescriptorSetHandles.resize(NumSets);
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

// 355
template <bool bUseDynamicGlobalUBs>
bool GraphicsPipelineDescriptorState::InternalUpdateDescriptorSets(CommandListContext *CmdListContext, CmdBuffer *CmdBuffer)
{
	// Early exit
	if (!UsedSetsMask)
	{
		return false;
	}

	VulkanUniformBufferUploader *UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8 *CPURingBufferBase = (uint8 *)UniformBufferUploader->GetCPUMappedPointer();
	const VkDeviceSize UBOffsetAlignment = device->GetLimits().minUniformBufferOffsetAlignment;

	const DescriptorSetRemappingInfo *RemappingInfo = PipelineDescriptorInfo->RemappingInfo;

	// Process updates
	{
		for (int32 Stage = 0; Stage < ShaderStage::NumStages; ++Stage)
		{
			if (PackedUniformBuffersDirty[Stage] != 0)
			{
				check(0);
				/* const uint32 DescriptorSet = RemappingInfo->StageInfos[Stage].PackedUBDescriptorSet;
				MarkDirty(UpdatePackedUniformBuffers<bUseDynamicGlobalUBs>(UBOffsetAlignment,
																		   RemappingInfo->StageInfos[Stage].PackedUBBindingIndices.data(),
																		   PackedUniformBuffers[Stage],
																		   DSWriter[DescriptorSet],
																		   UniformBufferUploader,
																		   CPURingBufferBase,
																		   PackedUniformBuffersDirty[Stage],
																		   CmdBuffer));
				PackedUniformBuffersDirty[Stage] = 0; */
			}
		}
	}

	if (UseVulkanDescriptorCache() && !HasVolatileResources())
	{
		check(0);
		/* if (bIsResourcesDirty)
		{
			device->GetDescriptorSetCache().GetDescriptorSets(GetDSetsKey(), *DescriptorSetsLayout, DSWriter, DescriptorSetHandles.GetData());
			bIsResourcesDirty = false;
		} */
	}
	else
	{
		const bool bNeedsWrite = (bIsResourcesDirty || ShouldAlwaysWriteDescriptors());

		// Allocate sets based on what changed
		if (CmdBuffer->AcquirePoolSetAndDescriptorsIfNeeded(*descriptorSetsLayout, bNeedsWrite, DescriptorSetHandles.data()))
		{
			uint32 RemainingSetsMask = UsedSetsMask;
			uint32 Set = 0;
			uint32 NumSets = 0;
			while (RemainingSetsMask)
			{
				if (RemainingSetsMask & 1)
				{
					const VkDescriptorSet DescriptorSet = DescriptorSetHandles[Set];
					DSWriter[Set].SetDescriptorSet(DescriptorSet);
					++NumSets;
				}

				++Set;
				RemainingSetsMask >>= 1;
			}
			vkUpdateDescriptorSets(device->GetInstanceHandle(), DSWriteContainer.DescriptorWrites.size(),
								   DSWriteContainer.DescriptorWrites.data(), 0, nullptr);

			bIsResourcesDirty = false;
		}
	}

	return true;
}

template bool GraphicsPipelineDescriptorState::InternalUpdateDescriptorSets<true>(CommandListContext *CmdListContext, CmdBuffer *CmdBuffer);
template bool GraphicsPipelineDescriptorState::InternalUpdateDescriptorSets<false>(CommandListContext *CmdListContext, CmdBuffer *CmdBuffer);
template bool ComputePipelineDescriptorState::InternalUpdateDescriptorSets<true>(CommandListContext *CmdListContext, CmdBuffer *CmdBuffer);
template bool ComputePipelineDescriptorState::InternalUpdateDescriptorSets<false>(CommandListContext *CmdListContext, CmdBuffer *CmdBuffer);