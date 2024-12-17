#pragma once

#include <vector>
#include <memory>
#include <iostream>
#include "Volk/volk.h"
#include "core/pixel_format.h"
#include "RHI/RHIDefinitions.h"
#include "RHI/RHIResources.h"
#include "common.h"
#include "core/assertion_macros.h"
class CmdBuffer;
class Device;
class RenderPass;
class VulkanView;

template <typename BitsType>
constexpr bool VKHasAllFlags(VkFlags Flags, BitsType Contains)
{
	return (Flags & Contains) == Contains;
}

template <typename BitsType>
constexpr bool VKHasAnyFlags(VkFlags Flags, BitsType Contains)
{
	return (Flags & Contains) != 0;
}

extern VkFormat GVulkanSRGBFormat[PF_MAX];

inline VkFormat UEToVkTextureFormat(PixelFormat UEFormat, const bool bIsSRGB)
{
	if (bIsSRGB)
	{
		return GVulkanSRGBFormat[UEFormat];
	}
	else
	{
		return (VkFormat)GPixelFormats[UEFormat].PlatformFormat;
	}
}

/// Merge a depth and a stencil layout for drivers that don't support VK_KHR_separate_depth_stencil_layouts
inline VkImageLayout GetMergedDepthStencilLayout(VkImageLayout DepthLayout, VkImageLayout StencilLayout)
{
	if ((DepthLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) || (StencilLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL))
	{
		if (StencilLayout != DepthLayout)
		{
			printf("You can't merge transfer src layout without anything else than transfer src.\n");
			printf("You need either VK_KHR_separate_depth_stencil_layouts or GRHISupportsSeparateDepthStencilCopyAccess enabled.\n");
			printf("From File:%s Line:%d\n", __FILE__, __LINE__);
		}
		return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}

	if ((DepthLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) || (StencilLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL))
	{
		if (StencilLayout != DepthLayout)
		{
			printf("You can't merge transfer dst layout without anything else than transfer dst.\n");
			printf("You need either VK_KHR_separate_depth_stencil_layouts or GRHISupportsSeparateDepthStencilCopyAccess enabled.\n");
			printf("From File:%s Line:%d\n", __FILE__, __LINE__);
		}
		return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}

	if ((DepthLayout == VK_IMAGE_LAYOUT_UNDEFINED) && (StencilLayout == VK_IMAGE_LAYOUT_UNDEFINED))
	{
		return VK_IMAGE_LAYOUT_UNDEFINED;
	}

	// Depth formats used on textures that aren't targets (like GBlackTextureDepthCube)
	if ((DepthLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) && (StencilLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
	{
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	auto IsMergedLayout = [](VkImageLayout Layout)
	{
		return (Layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) ||
			   (Layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) ||
			   (Layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL) ||
			   (Layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);
	};

	if (IsMergedLayout(DepthLayout) || IsMergedLayout(StencilLayout))
	{
		if (StencilLayout != DepthLayout)
			printf("Fuck %s %d\n", __FILE__, __LINE__);
		// checkf(StencilLayout == DepthLayout,
		// 	   TEXT("Layouts were already merged but they are mismatched (%s != %s)."),
		// 	   VK_TYPE_TO_STRING(VkImageLayout, DepthLayout), VK_TYPE_TO_STRING(VkImageLayout, StencilLayout));
		return DepthLayout;
	}

	if (DepthLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
	{
		if ((StencilLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) || (StencilLayout == VK_IMAGE_LAYOUT_UNDEFINED))
		{
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			check(StencilLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
			return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
		}
	}
	else if (DepthLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)
	{
		if ((StencilLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) || (StencilLayout == VK_IMAGE_LAYOUT_UNDEFINED))
		{
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
		else
		{
			check(StencilLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
			return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
		}
	}
	else
	{
		return (StencilLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	}
}

// Transitions an image to the specified layout. This does not update the layout cached internally by the RHI; the calling code must do that explicitly via FVulkanCommandListContext::GetLayoutManager() if necessary.
void VulkanSetImageLayout(CmdBuffer *CmdBuffer, VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange &SubresourceRange);

// 90
inline VkShaderStageFlagBits UEFrequencyToVKStageBit(ShaderFrequency InStage)
{
	switch (InStage)
	{
	case SF_Vertex:
		return VK_SHADER_STAGE_VERTEX_BIT;
	case SF_Pixel:
		return VK_SHADER_STAGE_FRAGMENT_BIT;
	case SF_Geometry:
		return VK_SHADER_STAGE_GEOMETRY_BIT;
	case SF_Compute:
		return VK_SHADER_STAGE_COMPUTE_BIT;

#if VULKAN_RHI_RAYTRACING
	case SF_RayGen:
		return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	case SF_RayMiss:
		return VK_SHADER_STAGE_MISS_BIT_KHR;
	case SF_RayHitGroup:
		return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR; // vkrt todo: How to handle VK_SHADER_STAGE_ANY_HIT_BIT_KHR?
	case SF_RayCallable:
		return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
#endif // VULKAN_RHI_RAYTRACING

	default:
		check(false);
		break;
	}

	return VK_SHADER_STAGE_ALL;
}

// 143
class RenderTargetLayout
{
public:
	RenderTargetLayout(const GraphicsPipelineStateInitializer &Initializer);
	RenderTargetLayout(Device &InDevice, const RenderPassInfo &RPInfo,
					   VkImageLayout CurrentDepthLayout, VkImageLayout CurrentStencilLayout);

	inline uint32 GetRenderPassCompatibleHash() const
	{
		check(bCalculatedHash);
		return RenderPassCompatibleHash;
	}
	inline uint32 GetRenderPassFullHash() const
	{
		check(bCalculatedHash);
		return RenderPassFullHash;
	}

	inline const VkOffset2D &GetOffset2D() const { return Offset.Offset2D; }
	inline const VkOffset3D &GetOffset3D() const { return Offset.Offset3D; }
	inline const VkExtent2D &GetExtent2D() const { return Extent.Extent2D; }
	inline const VkExtent3D &GetExtent3D() const { return Extent.Extent3D; }
	inline const VkAttachmentDescription *GetAttachmentDescriptions() const { return Desc; }
	inline uint32 GetNumColorAttachments() const { return NumColorAttachments; }
	inline bool GetHasDepthStencil() const { return bHasDepthStencil != 0; }
	inline bool GetHasResolveAttachments() const { return bHasResolveAttachments != 0; }
	inline uint32 GetNumAttachmentDescriptions() const { return NumAttachmentDescriptions; }
	inline uint32 GetNumUsedClearValues() const { return NumUsedClearValues; }
	inline bool GetIsMultiView() const { return MultiViewCount != 0; }
	inline uint32 GetMultiViewCount() const { return MultiViewCount; }

	inline const VkAttachmentReference *GetColorAttachmentReferences() const
	{
		return NumColorAttachments > 0 ? ColorReferences : nullptr;
	}
	inline const VkAttachmentReference *GetResolveAttachmentReferences() const
	{
		return bHasResolveAttachments ? ResolveReferences : nullptr;
	}
	inline const VkAttachmentReference *GetDepthAttachmentReference() const
	{
		return bHasDepthStencil ? &DepthReference : nullptr;
	}
	inline const VkAttachmentReferenceStencilLayout *GetStencilAttachmentReference() const
	{
		return bHasDepthStencil ? &StencilReference : nullptr;
	}

	inline const VkAttachmentDescriptionStencilLayout *GetStencilDesc() const { return bHasDepthStencil ? &StencilDesc : nullptr; }
	inline const SubpassHint GetSubpassHint() const { return SubpassHint; }
	inline const VkSurfaceTransformFlagBitsKHR GetQCOMRenderPassTransform() const { return QCOMRenderPassTransform; }

protected:
	VkSurfaceTransformFlagBitsKHR QCOMRenderPassTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	VkAttachmentReference ColorReferences[MaxSimultaneousRenderTargets];
	VkAttachmentReference DepthReference;
	VkAttachmentReferenceStencilLayout StencilReference;
	VkAttachmentReference FragmentDensityReference;
	VkAttachmentReference ResolveReferences[MaxSimultaneousRenderTargets];

	// Depth goes in the "+1" slot and the Shading Rate texture goes in the "+2" slot.
	VkAttachmentDescription Desc[MaxSimultaneousRenderTargets * 2 + 2];
	VkAttachmentDescriptionStencilLayout StencilDesc;

	uint8 NumAttachmentDescriptions;
	uint8 NumColorAttachments;
	uint8 NumInputAttachments = 0;
	uint8 bHasDepthStencil;
	uint8 bHasResolveAttachments;
	uint8 bHasFragmentDensityAttachment;
	uint8 NumSamples;
	uint8 NumUsedClearValues;
	SubpassHint SubpassHint = SubpassHint::None;
	uint8 MultiViewCount;

	// Hash for a compatible RenderPass
	uint32 RenderPassCompatibleHash = 0;
	// Hash for the render pass including the load/store operations
	uint32 RenderPassFullHash = 0;

	union
	{
		VkOffset3D Offset3D;
		VkOffset2D Offset2D;
	} Offset;

	union
	{
		VkExtent3D Extent3D;
		VkExtent2D Extent2D;
	} Extent;

	template <typename T>
	void Memzero(T &t)
	{
		memset(&t, 0, sizeof(T));
	}

	inline void ResetAttachments()
	{
		Memzero(ColorReferences);
		Memzero(DepthReference);
		Memzero(FragmentDensityReference);
		Memzero(ResolveReferences);
		Memzero(Desc);
		Memzero(Offset);
		Memzero(Extent);

		ZeroVulkanStruct(StencilReference, VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT);
		ZeroVulkanStruct(StencilDesc, VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT);
	}

	RenderTargetLayout()
	{
		NumAttachmentDescriptions = 0;
		NumColorAttachments = 0;
		bHasDepthStencil = 0;
		bHasResolveAttachments = 0;
		bHasFragmentDensityAttachment = 0;
		NumSamples = 0;
		NumUsedClearValues = 0;
		MultiViewCount = 0;

		ResetAttachments();
	}

	bool bCalculatedHash = false;

	friend class PipelineStateCacheManager;
	friend struct GfxPipelineDesc;
};

// 265
class Framebuffer
{
public:
	Framebuffer(Device &Device, const SetRenderTargetsInfo &InRTInfo, const RenderTargetLayout &RTLayout,
				const RenderPass &RenderPass);
	~Framebuffer();

	bool Matches(const SetRenderTargetsInfo &RTInfo) const;

	void Destroy(Device &Device);

	VkFramebuffer GetHandle() { return framebuffer; }

	std::vector<std::unique_ptr<VulkanView>> OwnedTextureViews;
	std::vector<VulkanView const *> AttachmentTextureViews;

	// Copy from the Depth render target partial view
	VulkanView const *PartialDepthTextureView = nullptr;

	bool ContainsRenderTarget(VkImage Image) const
	{
		ensure(Image != VK_NULL_HANDLE);
		for (uint32 Index = 0; Index < NumColorAttachments; ++Index)
		{
			if (ColorRenderTargetImages[Index] == Image)
			{
				return true;
			}
		}

		return (DepthStencilRenderTargetImage == Image);
	}

	VkRect2D GetRenderArea() const { return RenderArea; }

private:
	VkFramebuffer framebuffer;
	VkRect2D RenderArea;
	// Unadjusted number of color render targets as in FRHISetRenderTargetsInfo
	uint32 NumColorRenderTargets;

	// Save image off for comparison, in case it gets aliased.
	uint32 NumColorAttachments;
	VkImage ColorRenderTargetImages[MaxSimultaneousRenderTargets];
	VkImage ColorResolveTargetImages[MaxSimultaneousRenderTargets];
	VkImage DepthStencilRenderTargetImage;
	VkImage FragmentDensityImage;

	// Predefined set of barriers, when executes ensuring all writes are finished
	std::vector<VkImageMemoryBarrier> WriteBarriers;
};

// 343
class RenderPass
{
public:
	inline const RenderTargetLayout &GetLayout() const { return Layout; }
	inline VkRenderPass GetHandle() const { return renderPass; }
	inline uint32 GetNumUsedClearValues() const { return NumUsedClearValues; }

private:
	friend class RenderPassManager;
	RenderPass(Device &Device, const RenderTargetLayout &RTLayout);
	~RenderPass();
	RenderTargetLayout Layout;
	VkRenderPass renderPass;
	uint32 NumUsedClearValues;
	Device &device;
};

namespace VulkanRHI
{
	class StagingBuffer;
	struct PendingBufferLock
	{
		StagingBuffer *stagingBuffer;
		uint32 Offset;
		uint32 Size;
		ResourceLockMode LockMode;
	};

	// 571
	static VkImageAspectFlags GetAspectMaskFromUEFormat(PixelFormat Format, bool bIncludeStencil, bool bIncludeDepth = true)
	{
		switch (Format)
		{
		case PF_X24_G8:
			return VK_IMAGE_ASPECT_STENCIL_BIT;
		case PF_DepthStencil:
			return (bIncludeDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) | (bIncludeStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
		case PF_ShadowDepth:
		case PF_D24:
			return VK_IMAGE_ASPECT_DEPTH_BIT;
		default:
			return VK_IMAGE_ASPECT_COLOR_BIT;
		}
	}

	static bool VulkanFormatHasStencil(VkFormat Format)
	{
		switch (Format)
		{
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
		case VK_FORMAT_S8_UINT:
			return true;
		default:
			return false;
		}
	}
}

// 658
static inline VkFormat UEToVkBufferFormat(VertexElementType Type)
{
	switch (Type)
	{
	case VET_Float1:
		return VK_FORMAT_R32_SFLOAT;
	case VET_Float2:
		return VK_FORMAT_R32G32_SFLOAT;
	case VET_Float3:
		return VK_FORMAT_R32G32B32_SFLOAT;
	case VET_PackedNormal:
		return VK_FORMAT_R8G8B8A8_SNORM;
	case VET_UByte4:
		return VK_FORMAT_R8G8B8A8_UINT;
	case VET_UByte4N:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case VET_Color:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case VET_Short2:
		return VK_FORMAT_R16G16_SINT;
	case VET_Short4:
		return VK_FORMAT_R16G16B16A16_SINT;
	case VET_Short2N:
		return VK_FORMAT_R16G16_SNORM;
	case VET_Half2:
		return VK_FORMAT_R16G16_SFLOAT;
	case VET_Half4:
		return VK_FORMAT_R16G16B16A16_SFLOAT;
	case VET_Short4N: // 4 X 16 bit word: normalized
		return VK_FORMAT_R16G16B16A16_SNORM;
	case VET_UShort2:
		return VK_FORMAT_R16G16_UINT;
	case VET_UShort4:
		return VK_FORMAT_R16G16B16A16_UINT;
	case VET_UShort2N: // 16 bit word normalized to (value/65535.0:value/65535.0:0:0:1)
		return VK_FORMAT_R16G16_UNORM;
	case VET_UShort4N: // 4 X 16 bit word unsigned: normalized
		return VK_FORMAT_R16G16B16A16_UNORM;
	case VET_Float4:
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	case VET_URGB10A2N:
		return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
	case VET_UInt:
		return VK_FORMAT_R32_UINT;
	default:
		break;
	}

	check(!"Undefined vertex-element format conversion");
	return VK_FORMAT_UNDEFINED;
}

inline bool UseVulkanDescriptorCache() { return false; }