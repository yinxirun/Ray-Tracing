#pragma once
#include "vulkan_memory.h"
#include "pipeline.h"
#include <iostream>

class CommandListContext;
class CommandBuffer;
class VulkanGraphicsPipelineState;

// Common Pipeline state
class CommonPipelineDescriptorState : public VulkanRHI::DeviceChild
{
public:
	CommonPipelineDescriptorState(Device *InDevice)
		: VulkanRHI::DeviceChild(InDevice), bUseBindless(InDevice->SupportsBindless())
	{
	}

	// 	virtual ~FVulkanCommonPipelineDescriptorState() {}

	// 	const FVulkanDSetsKey& GetDSetsKey() const
	// 	{
	// 		check(UseVulkanDescriptorCache());
	// 		if (bIsDSetsKeyDirty)
	// 		{
	// 			DSetsKey.GenerateFromData(DSWriteContainer.HashableDescriptorInfo.GetData(),
	// 				sizeof(FVulkanHashableDescriptorInfo) * DSWriteContainer.HashableDescriptorInfo.Num());
	// 			bIsDSetsKeyDirty = false;
	// 		}
	// 		return DSetsKey;
	// 	}

	// 	bool HasVolatileResources() const
	// 	{
	// 		for (const FVulkanDescriptorSetWriter& Writer : DSWriter)
	// 		{
	// 			if (Writer.bHasVolatileResources)
	// 			{
	// 				return true;
	// 			}
	// 		}
	// 		return false;
	// 	}

	// 	inline void MarkDirty(bool bDirty)
	// 	{
	// 		bIsResourcesDirty |= bDirty;
	// 		bIsDSetsKeyDirty |= bDirty;
	// 	}

	// 	void SetSRV(FVulkanCmdBuffer* CmdBuffer, bool bCompute, uint8 DescriptorSet, uint32 BindingIndex, FVulkanShaderResourceView* SRV);
	// 	void SetUAV(FVulkanCmdBuffer* CmdBuffer, bool bCompute, uint8 DescriptorSet, uint32 BindingIndex, FVulkanUnorderedAccessView* UAV);

	// 	inline void SetTexture(uint8 DescriptorSet, uint32 BindingIndex, const FVulkanTexture* Texture, VkImageLayout Layout)
	// 	{
	// 		check(!bUseBindless);
	// 		check(Texture && Texture->PartialView);

	// 		// If the texture doesn't support sampling, then we read it through a UAV
	// 		if (Texture->SupportsSampling())
	// 		{
	// 			MarkDirty(DSWriter[DescriptorSet].WriteImage(BindingIndex, Texture->PartialView->GetTextureView(), Layout));
	// 		}
	// 		else
	// 		{
	// 			MarkDirty(DSWriter[DescriptorSet].WriteStorageImage(BindingIndex, Texture->PartialView->GetTextureView(), Layout));
	// 		}
	// 	}

	// 	inline void SetSamplerState(uint8 DescriptorSet, uint32 BindingIndex, const FVulkanSamplerState* Sampler)
	// 	{
	// 		check(!bUseBindless);
	// 		check(Sampler && Sampler->Sampler != VK_NULL_HANDLE);
	// 		MarkDirty(DSWriter[DescriptorSet].WriteSampler(BindingIndex, *Sampler));
	// 	}

	// 	inline void SetInputAttachment(uint8 DescriptorSet, uint32 BindingIndex, const FVulkanView::FTextureView& TextureView, VkImageLayout Layout)
	// 	{
	// 		check(!bUseBindless);
	// 		MarkDirty(DSWriter[DescriptorSet].WriteInputAttachment(BindingIndex, TextureView, Layout));
	// 	}

	// 	template<bool bDynamic>
	// 	inline void SetUniformBuffer(uint8 DescriptorSet, uint32 BindingIndex, const FVulkanUniformBuffer* UniformBuffer)
	// 	{
	// 		const VulkanRHI::FVulkanAllocation& Allocation = UniformBuffer->Allocation;
	// 		VkDeviceSize Range = UniformBuffer->bUniformView ? PLATFORM_MAX_UNIFORM_BUFFER_RANGE : UniformBuffer->GetSize();

	// 		if (bDynamic)
	// 		{
	// 			MarkDirty(DSWriter[DescriptorSet].WriteDynamicUniformBuffer(BindingIndex, Allocation.GetBufferHandle(), Allocation.HandleId, 0, Range, UniformBuffer->GetOffset()));
	// 		}
	// 		else
	// 		{
	// 			MarkDirty(DSWriter[DescriptorSet].WriteUniformBuffer(BindingIndex, Allocation.GetBufferHandle(), Allocation.HandleId, UniformBuffer->GetOffset(), Range));
	// 		}
	// 	}

	// 	inline void SetUniformBufferDynamicOffset(uint8 DescriptorSet, uint32 BindingIndex, uint32 DynamicOffset)
	// 	{
	// 		const uint8 DynamicOffsetIndex = DSWriter[DescriptorSet].BindingToDynamicOffsetMap[BindingIndex];
	// 		DSWriter[DescriptorSet].DynamicOffsets[DynamicOffsetIndex] = DynamicOffset;
	// 	}

protected:
	void Reset()
	{
#ifdef PRINT_UNIMPLEMENT
		printf("Have not implement CommonPipelineDescriptorState::Reset %s %d\n", __FILE__, __LINE__);
// 		for(FVulkanDescriptorSetWriter& Writer : DSWriter)
// 		{
// 			Writer.Reset();
// 		}
#endif
	}
	// 	inline void Bind(VkCommandBuffer CmdBuffer, VkPipelineLayout PipelineLayout, VkPipelineBindPoint BindPoint)
	// 	{
	// 		// Bindless will replace with global sets
	// 		if (!bUseBindless)
	// 		{
	// 			VulkanRHI::vkCmdBindDescriptorSets(CmdBuffer,
	// 				BindPoint,
	// 				PipelineLayout,
	// 				0, DescriptorSetHandles.Num(), DescriptorSetHandles.GetData(),
	// 				(uint32)DynamicOffsets.Num(), DynamicOffsets.GetData());
	// 		}
	// 	}

	void CreateDescriptorWriteInfos();

	// 	//#todo-rco: Won't work multithreaded!
	// 	FVulkanDescriptorSetWriteContainer DSWriteContainer;
	const DescriptorSetsLayout *descriptorSetsLayout = nullptr;

	// 	//#todo-rco: Won't work multithreaded!
	// 	TArray<VkDescriptorSet> DescriptorSetHandles;

	// 	// Bitmask of sets that exist in this pipeline
	// #todo-rco: Won't work multithreaded!
	uint32 UsedSetsMask = 0;

	// 	//#todo-rco: Won't work multithreaded!
	// 	TArray<uint32> DynamicOffsets;

	bool bIsResourcesDirty = true;

	// 	TArray<FVulkanDescriptorSetWriter> DSWriter;

	// 	mutable FVulkanDSetsKey DSetsKey;
	// 	mutable bool bIsDSetsKeyDirty = true;

	const bool bUseBindless;
};

class GraphicsPipelineDescriptorState : public CommonPipelineDescriptorState
{
public:
	GraphicsPipelineDescriptorState(Device *InDevice, VulkanGraphicsPipelineState *InGfxPipeline);
	virtual ~GraphicsPipelineDescriptorState()
	{
		GfxPipeline->Release();
	}

	// 	inline void SetPackedGlobalShaderParameter(uint8 Stage, uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue)
	// 	{
	// 		PackedUniformBuffers[Stage].SetPackedGlobalParameter(BufferIndex, ByteOffset, NumBytes, NewValue, PackedUniformBuffersDirty[Stage]);
	// 	}

	// 	inline void SetUniformBufferConstantData(uint8 Stage, uint32 BindingIndex, const TArray<uint8>& ConstantData)
	// 	{
	// 		PackedUniformBuffers[Stage].SetEmulatedUniformBufferIntoPacked(BindingIndex, ConstantData, PackedUniformBuffersDirty[Stage]);
	// 	}

	bool UpdateDescriptorSets(CommandListContext *CmdListContext, CmdBuffer *CmdBuffer)
	{
#ifdef PRINT_UNIMPLEMENT
		printf("Have not implement UpdateDescriptorSets %s\n", __FILE__);
#endif
		// 		check(!bUseBindless);

		// 		const bool bUseDynamicGlobalUBs = (GDynamicGlobalUBs->GetInt() > 0);
		// 		if (bUseDynamicGlobalUBs)
		// 		{
		// 			return InternalUpdateDescriptorSets<true>(CmdListContext, CmdBuffer);
		// 		}
		// 		else
		// 		{
		// 			return InternalUpdateDescriptorSets<false>(CmdListContext, CmdBuffer);
		// 		}
		return false;
	}

	// 	void UpdateBindlessDescriptors(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);

	inline void BindDescriptorSets(VkCommandBuffer CmdBuffer)
	{
		printf("ERROR %s\n", __FILE__);
		// 		Bind(CmdBuffer, GfxPipeline->GetLayout().GetPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS);
	}

	void Reset()
	{
		memcpy(PackedUniformBuffersDirty.data(), PackedUniformBuffersMask.data(), sizeof(uint64) * ShaderStage::NumStages);
		CommonPipelineDescriptorState::Reset();
		bIsResourcesDirty = true;
	}

	inline const VulkanGfxPipelineDescriptorInfo &GetGfxPipelineDescriptorInfo() const { return *PipelineDescriptorInfo; }

protected:
	const VulkanGfxPipelineDescriptorInfo *PipelineDescriptorInfo;

	std::array<PackedUniformBuffers, ShaderStage::NumStages> PackedUniformBuffers;
	std::array<uint64, ShaderStage::NumStages> PackedUniformBuffersMask;
	std::array<uint64, ShaderStage::NumStages> PackedUniformBuffersDirty;

	VulkanGraphicsPipelineState *GfxPipeline;

	// 	template<bool bUseDynamicGlobalUBs>
	// 	bool InternalUpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);

	friend class PendingGfxState;
	// 	friend class FVulkanCommandListContext;
};
