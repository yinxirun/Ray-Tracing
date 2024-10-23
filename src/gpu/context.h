#pragma once

#include "window.h"
#include "vulkan_base.h"

#include <memory>
#include <fstream>

class Context
{
public:
    std::unique_ptr<VulkanBase> vulkan_base;
    std::unique_ptr<Window> window;
    bool framebufferResized = false;

    Context()
    {
        window = std::make_unique<Window>();
        window->InitWindow(this);

        vulkan_base = std::make_unique<VulkanBase>();
        vulkan_base->initVulkan(this);
    }

    ~Context()
    {
        vulkan_base->cleanup();
        window->cleanup();
    }

    VkCommandBuffer BeginFrame()
    {
        return vulkan_base->BeginFrame(*window);
    }

    void EndFrame()
    {
        vulkan_base->EndFrame(framebufferResized, *window);
    }

    VkCommandBuffer BeginSingleTimeCommands()
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = vulkan_base->GetCommandPool();
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(vulkan_base->GetDevice(), &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer)
    {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(vulkan_base->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(vulkan_base->GetGraphicsQueue());

        vkFreeCommandBuffers(vulkan_base->GetDevice(), vulkan_base->GetCommandPool(), 1, &commandBuffer);
    }

    VkShaderModule CreateShaderModule(std::string filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = buffer.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(buffer.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(vulkan_base->GetDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    void DestroyShaderModule(VkShaderModule shaderModule)
    {
        vkDestroyShaderModule(vulkan_base->GetDevice(), shaderModule, nullptr);
    }

    VkPipelineLayout CreatePipelineLayout(VkPipelineLayoutCreateInfo ci)
    {
        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(vulkan_base->GetDevice(), &ci, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create compute pipeline layout!");
        }
        return pipelineLayout;
    }

    void DestroyPipelineLayout(VkPipelineLayout pipelineLayout)
    {
        vkDestroyPipelineLayout(vulkan_base->GetDevice(), pipelineLayout, nullptr);
    }

    VkPipeline CreateComputePipeline(VkComputePipelineCreateInfo ci)
    {
        VkPipeline pipelien;
        if (vkCreateComputePipelines(vulkan_base->GetDevice(), VK_NULL_HANDLE, 1, &ci, nullptr, &pipelien) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create compute pipeline!");
        }
        return pipelien;
    }

    void DestroyPipeline(VkPipeline pipeline)
    {
        vkDestroyPipeline(vulkan_base->GetDevice(), pipeline, nullptr);
    }

    VkDescriptorSetLayout CreateDescriptorSetLayout(VkDescriptorSetLayoutCreateInfo ci)
    {
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        vkCreateDescriptorSetLayout(vulkan_base->GetDevice(), &ci, nullptr, &descriptorSetLayout);
        return descriptorSetLayout;
    }

    void DestroyDescriptorSetLayout(VkDescriptorSetLayout descriptorSetLayout)
    {
        vkDestroyDescriptorSetLayout(vulkan_base->GetDevice(), descriptorSetLayout, nullptr);
    }

    VkDevice GetDevice() { return vulkan_base->GetDevice(); }
};