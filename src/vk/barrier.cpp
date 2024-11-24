#include "barrier.h"
#include "Volk/volk.h"
#include "command_buffer.h"
#include "private.h"
#include "resources.h"
#include "core/assertion_macros.h"
#include "context.h"
#include "queue.h"

int32 GVulkanAutoCorrectExpectedLayouts = 1;

// The following two functions are used when the RHI needs to do image layout transitions internally.
// They are not used for the transitions requested through the public API (RHICreate/Begin/EndTransition)
// unless the initial state in ERHIAccess::Unknown, in which case the tracking code kicks in.
static VkAccessFlags GetVkAccessMaskForLayout(const VkImageLayout Layout)
{
    VkAccessFlags Flags = 0;

    switch (Layout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        Flags = VK_ACCESS_TRANSFER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        Flags = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        Flags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
        Flags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
        Flags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        Flags = VK_ACCESS_SHADER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
        Flags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        Flags = 0;
        break;

    case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
        Flags = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
        break;

    case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
        Flags = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
        break;

    case VK_IMAGE_LAYOUT_GENERAL:
        // todo-jn: could be used for R64 in read layout
    case VK_IMAGE_LAYOUT_UNDEFINED:
        Flags = 0;
        break;

    case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
        // todo-jn: sync2 currently only used by depth/stencil targets
        Flags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
        // todo-jn: sync2 currently only used by depth/stencil targets
        Flags = VK_ACCESS_SHADER_READ_BIT;
        break;

    default:
        throw std::exception();
        break;
    }

    return Flags;
}

static VkPipelineStageFlags GetVkStageFlagsForLayout(VkImageLayout Layout)
{
    VkPipelineStageFlags Flags = 0;

    switch (Layout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        Flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        Flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        Flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
        Flags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        Flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
        Flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        break;

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        Flags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        break;

    case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
        Flags = VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;
        break;

    case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
        Flags = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
        break;

    case VK_IMAGE_LAYOUT_GENERAL:
    case VK_IMAGE_LAYOUT_UNDEFINED:
        Flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        break;

    case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
        // todo-jn: sync2 currently only used by depth/stencil targets
        Flags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        break;

    case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
        // todo-jn: sync2 currently only used by depth/stencil targets
        Flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        break;

    default:
        throw std::exception();
        break;
    }

    return Flags;
}

// Helpers for filling in the fields of a VkImageMemoryBarrier structure.
static void SetupImageBarrier(VkImageMemoryBarrier2 &ImgBarrier, VkImage Image, VkPipelineStageFlags SrcStageFlags, VkPipelineStageFlags DstStageFlags,
                              VkAccessFlags SrcAccessFlags, VkAccessFlags DstAccessFlags, VkImageLayout SrcLayout, VkImageLayout DstLayout, const VkImageSubresourceRange &SubresRange)
{
    ImgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    ImgBarrier.pNext = nullptr;
    ImgBarrier.srcStageMask = SrcStageFlags;
    ImgBarrier.dstStageMask = DstStageFlags;
    ImgBarrier.srcAccessMask = SrcAccessFlags;
    ImgBarrier.dstAccessMask = DstAccessFlags;
    ImgBarrier.oldLayout = SrcLayout;
    ImgBarrier.newLayout = DstLayout;
    ImgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ImgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ImgBarrier.image = Image;
    ImgBarrier.subresourceRange = SubresRange;
}

static void DowngradeBarrier(VkMemoryBarrier &OutBarrier, const VkMemoryBarrier2 &InBarrier)
{
    OutBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    OutBarrier.pNext = InBarrier.pNext;
    OutBarrier.srcAccessMask = InBarrier.srcAccessMask;
    OutBarrier.dstAccessMask = InBarrier.dstAccessMask;
}

static void DowngradeBarrier(VkBufferMemoryBarrier &OutBarrier, const VkBufferMemoryBarrier2 &InBarrier)
{
    OutBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    OutBarrier.pNext = InBarrier.pNext;
    OutBarrier.srcAccessMask = InBarrier.srcAccessMask;
    OutBarrier.dstAccessMask = InBarrier.dstAccessMask;
    OutBarrier.srcQueueFamilyIndex = InBarrier.srcQueueFamilyIndex;
    OutBarrier.dstQueueFamilyIndex = InBarrier.dstQueueFamilyIndex;
    OutBarrier.buffer = InBarrier.buffer;
    OutBarrier.offset = InBarrier.offset;
    OutBarrier.size = InBarrier.size;
}

static void DowngradeBarrier(VkImageMemoryBarrier &OutBarrier, const VkImageMemoryBarrier2 &InBarrier)
{
    OutBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    OutBarrier.pNext = InBarrier.pNext;
    OutBarrier.srcAccessMask = InBarrier.srcAccessMask;
    OutBarrier.dstAccessMask = InBarrier.dstAccessMask;
    OutBarrier.oldLayout = InBarrier.oldLayout;
    OutBarrier.newLayout = InBarrier.newLayout;
    OutBarrier.srcQueueFamilyIndex = InBarrier.srcQueueFamilyIndex;
    OutBarrier.dstQueueFamilyIndex = InBarrier.dstQueueFamilyIndex;
    OutBarrier.image = InBarrier.image;
    OutBarrier.subresourceRange = InBarrier.subresourceRange;
}

template <typename DstArrayType, typename SrcArrayType>
static void DowngradeBarrierArray(DstArrayType &TargetArray, const SrcArrayType &SrcArray, VkPipelineStageFlags &MergedSrcStageMask, VkPipelineStageFlags &MergedDstStageMask)
{
    TargetArray.reserve(TargetArray.size() + SrcArray.size());
    for (const auto &SrcBarrier : SrcArray)
    {
        TargetArray.resize(TargetArray.size() + 1);
        auto &DstBarrier = TargetArray.back();
        DowngradeBarrier(DstBarrier, SrcBarrier);
        MergedSrcStageMask |= SrcBarrier.srcStageMask;
        MergedDstStageMask |= SrcBarrier.dstStageMask;
    }
}

// Legacy manual barriers inside the RHI with FVulkanPipelineBarrier don't have access to tracking, assume same layout for both aspects
template <typename BarrierArrayType>
static void MergeDepthStencilLayouts(BarrierArrayType &TargetArray)
{
    for (auto &Barrier : TargetArray)
    {
        if (VKHasAnyFlags(Barrier.subresourceRange.aspectMask, (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)))
        {
            if (Barrier.newLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)
            {
                Barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            }
            else if (Barrier.newLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
            {
                Barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }

            if (Barrier.oldLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)
            {
                Barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            }
            else if (Barrier.oldLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
            {
                Barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
        }
    }
}

void PipelineBarrier::AddMemoryBarrier(VkAccessFlags InSrcAccessFlags, VkAccessFlags InDstAccessFlags,
                                       VkPipelineStageFlags InSrcStageMask, VkPipelineStageFlags InDstStageMask)
{
    const VkAccessFlags ReadMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT |
                                   VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT |
                                   VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
                                   VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                   VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT;

    if (MemoryBarriers.size() == 0)
    {
        MemoryBarriers.push_back(VkMemoryBarrier2{});
        VkMemoryBarrier2 &NewBarrier = MemoryBarriers.back();
        ZeroVulkanStruct(NewBarrier, VK_STRUCTURE_TYPE_MEMORY_BARRIER_2);
    }

    // Mash everything into a single barrier
    VkMemoryBarrier2 &MemoryBarrier = MemoryBarriers[0];

    // We only need a memory barrier if the previous commands wrote to the buffer. In case of a transition from read, an execution barrier is enough.
    const bool SrcAccessIsRead = ((InSrcAccessFlags & (~ReadMask)) == 0);
    if (!SrcAccessIsRead)
    {
        MemoryBarrier.srcAccessMask |= InSrcAccessFlags;
        MemoryBarrier.dstAccessMask |= InDstAccessFlags;
    }

    MemoryBarrier.srcStageMask |= InSrcStageMask;
    MemoryBarrier.dstStageMask |= InDstStageMask;
}

void PipelineBarrier::AddFullImageLayoutTransition(const VulkanTexture &Texture, VkImageLayout SrcLayout, VkImageLayout DstLayout)
{
    const VkPipelineStageFlags SrcStageMask = GetVkStageFlagsForLayout(SrcLayout);
    const VkPipelineStageFlags DstStageMask = GetVkStageFlagsForLayout(DstLayout);

    const VkAccessFlags SrcAccessFlags = GetVkAccessMaskForLayout(SrcLayout);
    const VkAccessFlags DstAccessFlags = GetVkAccessMaskForLayout(DstLayout);

    const VkImageSubresourceRange SubresourceRange = MakeSubresourceRange(Texture.GetFullAspectMask());
    if (Texture.IsDepthOrStencilAspect())
    {
        SrcLayout = GetMergedDepthStencilLayout(SrcLayout, SrcLayout);
        DstLayout = GetMergedDepthStencilLayout(DstLayout, DstLayout);
    }

    ImageBarriers.push_back({});
    VkImageMemoryBarrier2 &ImgBarrier = ImageBarriers.back();
    SetupImageBarrier(ImgBarrier, Texture.Image, SrcStageMask, DstStageMask, SrcAccessFlags, DstAccessFlags, SrcLayout, DstLayout, SubresourceRange);
}

void PipelineBarrier::AddImageLayoutTransition(VkImage Image, VkImageLayout SrcLayout, VkImageLayout DstLayout, const VkImageSubresourceRange &SubresourceRange)
{
    const VkPipelineStageFlags SrcStageMask = GetVkStageFlagsForLayout(SrcLayout);
    const VkPipelineStageFlags DstStageMask = GetVkStageFlagsForLayout(DstLayout);

    const VkAccessFlags SrcAccessFlags = GetVkAccessMaskForLayout(SrcLayout);
    const VkAccessFlags DstAccessFlags = GetVkAccessMaskForLayout(DstLayout);

    ImageBarriers.push_back(VkImageMemoryBarrier2{});
    VkImageMemoryBarrier2 &ImgBarrier = ImageBarriers.back();
    SetupImageBarrier(ImgBarrier, Image, SrcStageMask, DstStageMask, SrcAccessFlags, DstAccessFlags, SrcLayout, DstLayout, SubresourceRange);
}

void PipelineBarrier::AddImageLayoutTransition(VkImage Image, VkImageAspectFlags AspectMask,
                                               const struct ImageLayout &SrcLayout, VkImageLayout DstLayout)
{
    if (SrcLayout.AreAllSubresourcesSameLayout())
    {
        AddImageLayoutTransition(Image, SrcLayout.MainLayout, DstLayout, MakeSubresourceRange(AspectMask));
        return;
    }

    const VkPipelineStageFlags DstStageMask = GetVkStageFlagsForLayout(DstLayout);
    const VkAccessFlags DstAccessFlags = GetVkAccessMaskForLayout(DstLayout);

    auto Function = [&](VkImageAspectFlagBits SingleAspect)
    {
        VkImageSubresourceRange SubresourceRange = MakeSubresourceRange(SingleAspect, 0, 1, 0, 1);
        for (; SubresourceRange.baseArrayLayer < SrcLayout.NumLayers; ++SubresourceRange.baseArrayLayer)
        {
            for (SubresourceRange.baseMipLevel = 0; SubresourceRange.baseMipLevel < SrcLayout.NumMips; ++SubresourceRange.baseMipLevel)
            {
                const VkImageLayout SubresourceLayout = SrcLayout.GetSubresLayout(SubresourceRange.baseArrayLayer, SubresourceRange.baseMipLevel, SingleAspect);
                if (SubresourceLayout != DstLayout)
                {
                    const VkPipelineStageFlags SrcStageMask = GetVkStageFlagsForLayout(SubresourceLayout);
                    const VkAccessFlags SrcAccessFlags = GetVkAccessMaskForLayout(SubresourceLayout);

                    ImageBarriers.push_back(VkImageMemoryBarrier2{});
                    VkImageMemoryBarrier2 &ImgBarrier = ImageBarriers.back();
                    SetupImageBarrier(ImgBarrier, Image, SrcStageMask, DstStageMask, SrcAccessFlags, DstAccessFlags, SubresourceLayout, DstLayout, SubresourceRange);
                }
            }
        }
    };

    for (uint32 SingleBit = 1; SingleBit <= VK_IMAGE_ASPECT_STENCIL_BIT; SingleBit <<= 1)
    {
        if ((AspectMask & SingleBit) != 0)
        {
            Function((VkImageAspectFlagBits)SingleBit);
        }
    }
}

void PipelineBarrier::Execute(CmdBuffer *CmdBuffer)
{
    if (MemoryBarriers.size() != 0 || BufferBarriers.size() != 0 || ImageBarriers.size() != 0)
    {
        if (CmdBuffer->GetDevice()->SupportsParallelRendering())
        {
            VkDependencyInfo DependencyInfo;
            DependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            DependencyInfo.pNext = nullptr;
            DependencyInfo.dependencyFlags = 0;
            DependencyInfo.memoryBarrierCount = MemoryBarriers.size();
            DependencyInfo.pMemoryBarriers = MemoryBarriers.data();
            DependencyInfo.bufferMemoryBarrierCount = BufferBarriers.size();
            DependencyInfo.pBufferMemoryBarriers = BufferBarriers.data();
            DependencyInfo.imageMemoryBarrierCount = ImageBarriers.size();
            DependencyInfo.pImageMemoryBarriers = ImageBarriers.data();
            vkCmdPipelineBarrier2KHR(CmdBuffer->GetHandle(), &DependencyInfo);
        }
        else
        {
            // Call the original execute with older types
            Execute(CmdBuffer->GetHandle());
        }
    }
}

void PipelineBarrier::Execute(VkCommandBuffer CmdBuffer)
{
    if (MemoryBarriers.size() != 0 || BufferBarriers.size() != 0 || ImageBarriers.size() != 0)
    {
        VkPipelineStageFlags SrcStageMask = 0;
        VkPipelineStageFlags DstStageMask = 0;

        std::vector<VkMemoryBarrier> TempMemoryBarriers;
        DowngradeBarrierArray(TempMemoryBarriers, MemoryBarriers, SrcStageMask, DstStageMask);

        std::vector<VkBufferMemoryBarrier> TempBufferBarriers;
        DowngradeBarrierArray(TempBufferBarriers, BufferBarriers, SrcStageMask, DstStageMask);

        std::vector<VkImageMemoryBarrier> TempImageBarriers;
        DowngradeBarrierArray(TempImageBarriers, ImageBarriers, SrcStageMask, DstStageMask);
        MergeDepthStencilLayouts(TempImageBarriers);

        vkCmdPipelineBarrier(CmdBuffer, SrcStageMask, DstStageMask, 0, TempMemoryBarriers.size(), TempMemoryBarriers.data(),
                             TempBufferBarriers.size(), TempBufferBarriers.data(), TempImageBarriers.size(), TempImageBarriers.data());
    }
}

VkImageSubresourceRange PipelineBarrier::MakeSubresourceRange(VkImageAspectFlags AspectMask, uint32_t FirstMip, uint32_t NumMips, uint32_t FirstLayer, uint32_t NumLayers)
{
    VkImageSubresourceRange Range;
    Range.aspectMask = AspectMask;
    Range.baseMipLevel = FirstMip;
    Range.levelCount = NumMips;
    Range.baseArrayLayer = FirstLayer;
    Range.layerCount = NumLayers;
    return Range;
}

//
// Used when we need to change the layout of a single image.
// Some plug-ins call this function from outside the RHI (Steam VR, at the time of writing this).
//
void VulkanSetImageLayout(CmdBuffer *CmdBuffer, VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange &SubresourceRange)
{
    PipelineBarrier Barrier;
    Barrier.AddImageLayoutTransition(Image, OldLayout, NewLayout, SubresourceRange);
    Barrier.Execute(CmdBuffer);
}

bool ImageLayout::AreSubresourcesSameLayout(VkImageLayout Layout, const VkImageSubresourceRange &SubresourceRange) const
{
    if (SubresLayouts.size() == 0)
    {
        return MainLayout == Layout;
    }

    printf("Fuck!! %s %d\n", __FILE__, __LINE__);

    const uint32 FirstPlane = (SubresourceRange.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT) ? NumPlanes - 1 : 0;
    const uint32 LastPlane = (SubresourceRange.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) ? NumPlanes : 1;

    const uint32 FirstLayer = SubresourceRange.baseArrayLayer;
    const uint32 LastLayer = FirstLayer + GetSubresRangeLayerCount(SubresourceRange);

    const uint32 FirstMip = SubresourceRange.baseMipLevel;
    const uint32 LastMip = FirstMip + GetSubresRangeMipCount(SubresourceRange);

    for (uint32 PlaneIdx = FirstPlane; PlaneIdx < LastPlane; ++PlaneIdx)
    {
        for (uint32 LayerIdx = FirstLayer; LayerIdx < LastLayer; ++LayerIdx)
        {
            for (uint32 MipIdx = FirstMip; MipIdx < LastMip; ++MipIdx)
            {
                if (SubresLayouts[(PlaneIdx * NumLayers * NumMips) + (LayerIdx * NumMips) + MipIdx] != Layout)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

void ImageLayout::CollapseSubresLayoutsIfSame()
{
    if (SubresLayouts.size() == 0)
    {
        return;
    }

    const VkImageLayout Layout = SubresLayouts[0];
    for (uint32 i = 1; i < NumPlanes * NumLayers * NumMips; ++i)
    {
        if (SubresLayouts[i] != Layout)
        {
            return;
        }
    }

    MainLayout = Layout;
    SubresLayouts.clear();
}

void ImageLayout::Set(VkImageLayout Layout, const VkImageSubresourceRange &SubresourceRange)
{
    check((Layout != VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL) &&
          (Layout != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL) &&
          (Layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) &&
          (Layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL));

    const uint32 FirstPlane = (SubresourceRange.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT) ? NumPlanes - 1 : 0;
    const uint32 LastPlane = (SubresourceRange.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) ? NumPlanes : 1;

    const uint32 FirstLayer = SubresourceRange.baseArrayLayer;
    const uint32 LayerCount = GetSubresRangeLayerCount(SubresourceRange);

    const uint32 FirstMip = SubresourceRange.baseMipLevel;
    const uint32 MipCount = GetSubresRangeMipCount(SubresourceRange);

    if (FirstPlane == 0 && LastPlane == NumPlanes &&
        FirstLayer == 0 && LayerCount == NumLayers &&
        FirstMip == 0 && MipCount == NumMips)
    {
        // We're setting the entire resource to the same layout.
        MainLayout = Layout;
        SubresLayouts.clear();
        return;
    }

    if (SubresLayouts.size() == 0)
    {
        const uint32 SubresLayoutCount = NumPlanes * NumLayers * NumMips;
        SubresLayouts.resize(SubresLayoutCount);
        for (uint32 i = 0; i < SubresLayoutCount; ++i)
        {
            SubresLayouts[i] = MainLayout;
        }
    }

    for (uint32 Plane = FirstPlane; Plane < LastPlane; ++Plane)
    {
        for (uint32 Layer = FirstLayer; Layer < FirstLayer + LayerCount; ++Layer)
        {
            for (uint32 Mip = FirstMip; Mip < FirstMip + MipCount; ++Mip)
            {
                SubresLayouts[Plane * (NumLayers * NumMips) + Layer * NumMips + Mip] = Layout;
            }
        }
    }

    // It's possible we've just set all the subresources to the same layout. If that's the case, get rid of the
    // subresource info and set the main layout appropriatedly.
    CollapseSubresLayoutsIfSame();
}

void LayoutManager::NotifyDeletedImage(VkImage Image){
    Layouts.erase(Image);
}

VkImageLayout LayoutManager::GetDefaultLayout(CmdBuffer *CmdBuffer, const VulkanTexture &VulkanTexture, Access DesiredAccess)
{
    switch (DesiredAccess)
    {
    case Access::SRVCompute:
    case Access::SRVGraphics:
    case Access::SRVMask:
    {
        if (VulkanTexture.IsDepthOrStencilAspect())
        {
            const bool bSupportsParallelRendering = CmdBuffer->GetDevice()->SupportsParallelRendering();
            if (bSupportsParallelRendering)
            {
                return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
            }
            else
            {
                // The only layout that can't be hardcoded...  Even if we only use depth, we need to know stencil
                LayoutManager &LayoutMgr = CmdBuffer->GetLayoutManager();
                const VkImageLayout DepthLayout = LayoutMgr.GetDepthStencilHint(VulkanTexture, VK_IMAGE_ASPECT_DEPTH_BIT);
                const VkImageLayout StencilLayout = LayoutMgr.GetDepthStencilHint(VulkanTexture, VK_IMAGE_ASPECT_STENCIL_BIT);
                return GetMergedDepthStencilLayout(DepthLayout, StencilLayout);
            }
        }
        else
        {
            return VulkanTexture.SupportsSampling() ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
        }
    }

    case Access::UAVCompute:
    case Access::UAVGraphics:
    case Access::UAVMask:
        return VK_IMAGE_LAYOUT_GENERAL;

    case Access::CopySrc:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case Access::CopyDest:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    case Access::DSVRead:
        return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    case Access::DSVWrite:
        return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

    case Access::ShadingRateSource:
    {
        printf("ERROR: Don't support VSR %s %d\n", __FILE__, __LINE__);
        // if (GRHIVariableRateShadingImageDataType == VRSImage_Palette)
        // {
        //     return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
        // }
        // else if (GRHIVariableRateShadingImageDataType == VRSImage_Fractional)
        // {
        //     return VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
        // }
    }

    default:
        checkNoEntry();
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

VkImageLayout LayoutManager::SetExpectedLayout(CmdBuffer *CmdBuffer, const VulkanTexture &VulkanTexture, Access DesiredAccess)
{
    const VkImageLayout ExpectedLayout = GetDefaultLayout(CmdBuffer, VulkanTexture, DesiredAccess);
    VkImageLayout PreviousLayout = ExpectedLayout;

    LayoutManager &LayoutMgr = CmdBuffer->GetLayoutManager();

    // This code path is not safe for multi-threaded code
    if (!LayoutMgr.bWriteOnly || GVulkanAutoCorrectExpectedLayouts)
    {
        auto iter = LayoutMgr.Layouts.find(VulkanTexture.Image);
        const ImageLayout *SrcLayout = iter != LayoutMgr.Layouts.end() ? &iter->second : nullptr;

        if (!SrcLayout)
        {
            LayoutManager &QueueLayoutMgr = CmdBuffer->GetOwner()->GetMgr().GetCommandListContext()->GetQueue()->GetLayoutManager();

            auto iter = QueueLayoutMgr.Layouts.find(VulkanTexture.Image);
            SrcLayout = iter != QueueLayoutMgr.Layouts.end() ? &iter->second : nullptr;
        }

        if (SrcLayout && (!SrcLayout->AreAllSubresourcesSameLayout() || (SrcLayout->MainLayout != ExpectedLayout)))
        {
            PipelineBarrier Barrier;
            if (VulkanTexture.IsDepthOrStencilAspect() && (SrcLayout->NumPlanes > 1) && !LayoutMgr.bWriteOnly)
            {
                const VkImageLayout MergedLayout = GetMergedDepthStencilLayout(SrcLayout->GetSubresLayout(0, 0, VK_IMAGE_ASPECT_DEPTH_BIT), SrcLayout->GetSubresLayout(0, 0, VK_IMAGE_ASPECT_STENCIL_BIT));
                Barrier.AddImageLayoutTransition(VulkanTexture.Image, MergedLayout, ExpectedLayout, PipelineBarrier::MakeSubresourceRange(VulkanTexture.GetFullAspectMask()));
                PreviousLayout = MergedLayout;
            }
            else
            {
                Barrier.AddImageLayoutTransition(VulkanTexture.Image, VulkanTexture.GetFullAspectMask(), *SrcLayout, ExpectedLayout);
                check(SrcLayout->AreAllSubresourcesSameLayout());
                PreviousLayout = SrcLayout->MainLayout;
            }
            Barrier.Execute(CmdBuffer);
        }
    }

    return PreviousLayout;
}

VkImageLayout LayoutManager::GetDepthStencilHint(const VulkanTexture &VulkanTexture, VkImageAspectFlagBits AspectBit)
{
    check(AspectBit == VK_IMAGE_ASPECT_DEPTH_BIT || AspectBit == VK_IMAGE_ASPECT_STENCIL_BIT);
    auto iter = Layouts.find(VulkanTexture.Image);
    ImageLayout *Layout = iter != Layouts.end() ? &iter->second : nullptr;
    if (Layout)
    {
        return Layout->GetSubresLayout(0, 0, AspectBit);
    }
    else if (Fallback)
    {
        return Fallback->GetDepthStencilHint(VulkanTexture, AspectBit);
    }
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

void LayoutManager::TransferTo(LayoutManager &Destination)
{
    if (Layouts.size())
    {
        for (auto &pair : Layouts)
        {
            Destination.Layouts.insert(pair);
        }
        Layouts.clear();
    }
}

void SetImageLayout(CmdBuffer *CmdBuffer, VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange &SubresourceRange)
{
    PipelineBarrier Barrier;
    Barrier.AddImageLayoutTransition(Image, OldLayout, NewLayout, SubresourceRange);
    Barrier.Execute(CmdBuffer);
}