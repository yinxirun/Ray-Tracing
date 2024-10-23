#pragma once

#include "context.h"

class Image
{
public:
    Image(Context &c, VkDevice device, VmaAllocator allocator, VkFormat format, int width, int height, VkImageUsageFlags usage) : context(c), device(device), allocator(allocator)
    {
        extent.width = width;
        extent.height = height;
        extent.depth = 1;

        VkImageCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.extent = extent;
        ci.format = format;
        ci.arrayLayers = 1;
        ci.mipLevels = 1;
        ci.usage = usage;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ci.samples = VK_SAMPLE_COUNT_1_BIT;
        ci.imageType = VK_IMAGE_TYPE_2D;

        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = VMA_MEMORY_USAGE_AUTO;
        allocCI.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        VmaAllocationInfo allocInfo;
        vmaCreateImage(allocator, &ci, &allocCI, &handle, &allocation, &allocInfo);

        size = allocInfo.size;
    }

    void Upload(void *data)
    {
        VkBufferCreateInfo bufferCI{};
        bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCI.size = size;
        bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = VMA_MEMORY_USAGE_AUTO;
        allocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocation bufferAllocation;
        VmaAllocationInfo allocInfo;
        VkBuffer stagingBuffer;
        vmaCreateBuffer(allocator, &bufferCI, &allocCI, &stagingBuffer, &bufferAllocation, &allocInfo);

        memcpy(allocInfo.pMappedData, data, size);

        auto cb = context.BeginSingleTimeCommands();
        VkBufferImageCopy copy{};
        copy.bufferImageHeight = extent.height;
        copy.bufferOffset = 0;
        copy.bufferRowLength = 0;
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 1;
        copy.imageOffset = {0, 0, 0};
        copy.imageExtent = extent;
        vkCmdCopyBufferToImage(cb, stagingBuffer, handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        context.endSingleTimeCommands(cb);

        vmaDestroyBuffer(allocator, stagingBuffer, bufferAllocation);
    }

private:
    Context &context;
    VkDevice device;
    VmaAllocator allocator;

    VmaAllocation allocation;
    VkExtent3D extent;
    VkDeviceSize size;
    VkImage handle;
};