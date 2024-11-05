#pragma once
#include "Volk/volk.h"
#include <vector>
#include "gpu/RHI/RHIAccess.h"
#include "resources.h"
#include "gpu/core/assertion_macros.h"
#include <unordered_map>

class CmdBuffer;
class VulkanTexture;

namespace std
{
    template <>
    struct hash<VkImage>
    {
        size_t operator()(VkImage image) const
        {
            return (uint64)image;
        }
    };
}

struct PipelineBarrier
{
    using MemoryBarrierArrayType = std::vector<VkMemoryBarrier2>;
    using ImageBarrierArrayType = std::vector<VkImageMemoryBarrier2>;
    using BufferBarrierArrayType = std::vector<VkBufferMemoryBarrier2>;

    MemoryBarrierArrayType MemoryBarriers;
    ImageBarrierArrayType ImageBarriers;
    BufferBarrierArrayType BufferBarriers;

    void AddMemoryBarrier(VkAccessFlags SrcAccessFlags, VkAccessFlags DstAccessFlags,
                          VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask);

    void AddFullImageLayoutTransition(const VulkanTexture &Texture, VkImageLayout SrcLayout, VkImageLayout DstLayout);

    void AddImageLayoutTransition(VkImage Image, VkImageLayout SrcLayout,
                                  VkImageLayout DstLayout, const VkImageSubresourceRange &SubresourceRange);

    void AddImageLayoutTransition(VkImage Image, VkImageAspectFlags AspectMask,
                                  const struct ImageLayout &SrcLayout, VkImageLayout DstLayout);

    void Execute(CmdBuffer *CmdBuffer);

    void Execute(VkCommandBuffer CmdBuffer);

    static VkImageSubresourceRange MakeSubresourceRange(VkImageAspectFlags AspectMask, uint32_t FirstMip = 0, uint32_t NumMips = VK_REMAINING_MIP_LEVELS,
                                                        uint32_t FirstLayer = 0, uint32_t NumLayers = VK_REMAINING_ARRAY_LAYERS);
};

struct ImageLayout
{
    ImageLayout(VkImageLayout InitialLayout, uint32_t InNumMips,
                uint32_t InNumLayers, VkImageAspectFlags Aspect)
        : NumMips(InNumMips),
          NumLayers(InNumLayers),
          NumPlanes((Aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) ? 2 : 1),
          MainLayout(InitialLayout)
    {
    }

    uint32_t NumMips;
    uint32_t NumLayers;
    uint32_t NumPlanes;
    // The layout when all the subresources are in the same state.
    VkImageLayout MainLayout;

    // Explicit subresource layouts. Always NumLayers*NumMips elements.
    std::vector<VkImageLayout> SubresLayouts;

    inline bool AreAllSubresourcesSameLayout() const { return SubresLayouts.size() == 0; }

    VkImageLayout GetSubresLayout(uint32 Layer, uint32 Mip, VkImageAspectFlagBits Aspect) const
    {
        return GetSubresLayout(Layer, Mip, (Aspect == VK_IMAGE_ASPECT_STENCIL_BIT) ? NumPlanes - 1 : 0);
    }

    VkImageLayout GetSubresLayout(uint32 Layer, uint32 Mip, uint32 Plane) const
    {
        if (SubresLayouts.size() == 0)
        {
            return MainLayout;
        }

        if (Layer == (uint32)-1)
        {
            Layer = 0;
        }

        check(Plane < NumPlanes && Layer < NumLayers && Mip < NumMips);
        return SubresLayouts[(Plane * NumLayers * NumMips) + (Layer * NumMips) + Mip];
    }

    bool AreSubresourcesSameLayout(VkImageLayout Layout, const VkImageSubresourceRange &SubresourceRange) const;

    inline uint32 GetSubresRangeLayerCount(const VkImageSubresourceRange &SubresourceRange) const
    {
        check(SubresourceRange.baseArrayLayer < NumLayers);
        return (SubresourceRange.layerCount == VK_REMAINING_ARRAY_LAYERS) ? (NumLayers - SubresourceRange.baseArrayLayer) : SubresourceRange.layerCount;
    }

    inline uint32 GetSubresRangeMipCount(const VkImageSubresourceRange &SubresourceRange) const
    {
        check(SubresourceRange.baseMipLevel < NumMips);
        return (SubresourceRange.levelCount == VK_REMAINING_MIP_LEVELS) ? (NumMips - SubresourceRange.baseMipLevel) : SubresourceRange.levelCount;
    }

    void CollapseSubresLayoutsIfSame();

    void Set(VkImageLayout Layout, const VkImageSubresourceRange &SubresourceRange);
};

class LayoutManager
{
public:
    LayoutManager(bool InWriteOnly, LayoutManager *InFallback)
        : bWriteOnly(InWriteOnly), Fallback(InFallback) {}

    void NotifyDeletedImage(VkImage Image);

    // Predetermined layouts for given RHIAccess
    static VkImageLayout GetDefaultLayout(CmdBuffer *CmdBuffer, const VulkanTexture &VulkanTexture, Access DesiredAccess);

    // Expected layouts and Hints are workarounds until we can use 'hardcoded layouts' everywhere.
    static VkImageLayout SetExpectedLayout(CmdBuffer *CmdBuffer, const VulkanTexture &VulkanTexture, Access DesiredAccess);
    VkImageLayout GetDepthStencilHint(const VulkanTexture &VulkanTexture, VkImageAspectFlagBits AspectBit);

    const ImageLayout *GetFullLayout(VkImage Image) const
    {
        check(!bWriteOnly);
        auto iter = Layouts.find(Image);
        const ImageLayout *Layout = iter == Layouts.end() ? nullptr : &iter->second;
        if (!Layout && Fallback)
        {
            return Fallback->GetFullLayout(Image);
        }
        return Layout;
    }

    const ImageLayout *GetFullLayout(const VulkanTexture &VulkanTexture, bool bAddIfNotFound = false, VkImageLayout LayoutIfNotFound = VK_IMAGE_LAYOUT_UNDEFINED)
    {
        check(!bWriteOnly);

        auto iter = Layouts.find(VulkanTexture.Image);
        const ImageLayout *Layout = iter == Layouts.end() ? nullptr : &iter->second;

        if (!Layout && Fallback)
        {
            Layout = Fallback->GetFullLayout(VulkanTexture, false);

            // If the layout was found in the fallback, carry it forward to our current manager for future tracking
            if (Layout)
            {
                Layouts.insert(std::pair(VulkanTexture.Image, *Layout));
                return Layout;
            }
        }

        if (Layout)
        {
            return Layout;
        }
        else if (!bAddIfNotFound)
        {
            return nullptr;
        }

        Layouts.insert(std::pair(VulkanTexture.Image, ImageLayout(LayoutIfNotFound, VulkanTexture.GetDesc().NumMips, VulkanTexture.GetNumberOfArrayLevels(), VulkanTexture.GetFullAspectMask())));
        iter = Layouts.find(VulkanTexture.Image);
        return &iter->second;
    }

    void SetFullLayout(VkImage Image, const ImageLayout &NewLayout)
    {
        auto iter = Layouts.find(Image);
        ImageLayout *Layout = (iter == Layouts.end()) ? nullptr : &(iter->second);
        if (Layout)
        {
            *Layout = NewLayout;
        }
        else
        {
            Layouts.insert(std::pair(Image, NewLayout));
        }
    }

    void SetFullLayout(const VulkanTexture &VulkanTexture, VkImageLayout InLayout, bool bOnlyIfNotFound = false)
    {
        auto iter = Layouts.find(VulkanTexture.Image);
        ImageLayout *Layout = iter == Layouts.end() ? nullptr : &iter->second;
        if (Layout)
        {
            if (!bOnlyIfNotFound)
            {
                Layout->Set(InLayout, PipelineBarrier::MakeSubresourceRange(VulkanTexture.GetFullAspectMask()));
            }
        }
        else
        {
            Layouts.insert(std::pair(VulkanTexture.Image,
                                     ImageLayout(InLayout, VulkanTexture.GetDesc().NumMips,
                                                 VulkanTexture.GetNumberOfArrayLevels(),
                                                 VulkanTexture.GetFullAspectMask())));
        }
    }

    // Transfers our layouts into the destination
    void TransferTo(LayoutManager &Destination);

private:
    std::unordered_map<VkImage, ImageLayout> Layouts;
    // If we're WriteOnly, we should never read layout from this instance.  This is important for parallel rendering.
    // When WriteOnly, this instance of the layout manager should only collect layouts to later feed them to the another central mgr.
    const bool bWriteOnly;

    // If parallel command list creation is NOT supported, then the queue's layout mgr can be used as a fallback to fetch previous layouts.
    LayoutManager *Fallback;
};

void SetImageLayout(CmdBuffer *CmdBuffer, VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange &SubresourceRange);