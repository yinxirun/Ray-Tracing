#pragma once

#include "Volk/volk.h"
#include "gpu/core/pixel_format.h"

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
extern VkFormat GPixelFormats[PF_MAX];
inline VkFormat UEToVkTextureFormat(EPixelFormat UEFormat, const bool bIsSRGB)
{
	if (bIsSRGB)
	{
		return GVulkanSRGBFormat[UEFormat];
	}
	else
	{
		return (VkFormat)GPixelFormats[UEFormat];
	}
}

// Merge a depth and a stencil layout for drivers that don't support VK_KHR_separate_depth_stencil_layouts
inline VkImageLayout GetMergedDepthStencilLayout(VkImageLayout DepthLayout, VkImageLayout StencilLayout)
{
	if ((DepthLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) || (StencilLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL))
	{
		if (StencilLayout == DepthLayout)
			printf("Fuck %s %d\n", __FILE__, __LINE__);
		// checkf(StencilLayout == DepthLayout,
		// 	   TEXT("You can't merge transfer src layout without anything else than transfer src (%s != %s).  ")
		// 		   TEXT("You need either VK_KHR_separate_depth_stencil_layouts or GRHISupportsSeparateDepthStencilCopyAccess enabled."),
		// 	   VK_TYPE_TO_STRING(VkImageLayout, DepthLayout), VK_TYPE_TO_STRING(VkImageLayout, StencilLayout));
		return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}

	if ((DepthLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) || (StencilLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL))
	{
		if (StencilLayout == DepthLayout)
			printf("Fuck %s %d\n", __FILE__, __LINE__);
		// checkf(StencilLayout == DepthLayout,
		// 	   TEXT("You can't merge transfer dst layout without anything else than transfer dst (%s != %s).  ")
		// 		   TEXT("You need either VK_KHR_separate_depth_stencil_layouts or GRHISupportsSeparateDepthStencilCopyAccess enabled."),
		// 	   VK_TYPE_TO_STRING(VkImageLayout, DepthLayout), VK_TYPE_TO_STRING(VkImageLayout, StencilLayout));
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
		if (StencilLayout == DepthLayout)
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

namespace VulkanRHI
{
	// 571
	static VkImageAspectFlags GetAspectMaskFromUEFormat(EPixelFormat Format, bool bIncludeStencil, bool bIncludeDepth = true)
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
}