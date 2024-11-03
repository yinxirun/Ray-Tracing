#pragma once
#include "Volk/volk.h"
#define VK_NO_PROTOTYPES
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vma/vk_mem_alloc.h"
#include <cstdint>
#include <cassert>
#include <vector>
#include <array>
#include <iostream>
#include "common.h"
#include <cstring>
#include "vulkan_memory.h"
#include "gpu/core/pixel_format.h"

class Queue;
class RHI;
class CmdBuffer;
class CommandListContextImmediate;
class CommandListContext;
class DescriptorPoolsManager;
class BindlessDescriptorManager;
class Device;

extern const std::vector<const char *> deviceExtensions;
extern const std::vector<const char *> validationLayers;

class PhysicalDeviceFeatures
{
public:
    PhysicalDeviceFeatures()
    {
        memset(this, 0, sizeof(PhysicalDeviceFeatures));
    }

    void Query(VkPhysicalDevice PhysicalDevice, uint32_t APIVersion);

    VkPhysicalDeviceFeatures Core_1_0;
    VkPhysicalDeviceVulkan11Features Core_1_1;

private:
    // Anything above Core 1.1 cannot be assumed, they should only be used by the device at init time
    VkPhysicalDeviceVulkan12Features Core_1_2;
    VkPhysicalDeviceVulkan13Features Core_1_3;

    friend class FVulkanDevice;
};

namespace VulkanRHI
{
    class DeferredDeletionQueue2 : public DeviceChild
    {
    public:
        DeferredDeletionQueue2(Device *InDevice);
        ~DeferredDeletionQueue2();

        enum class EType
        {
            RenderPass,
            Buffer,
            BufferView,
            Image,
            ImageView,
            Pipeline,
            PipelineLayout,
            Framebuffer,
            DescriptorSetLayout,
            Sampler,
            Semaphore,
            ShaderModule,
            Event,
            ResourceAllocation,
            DeviceMemoryAllocation,
            BufferSuballocation,
            AccelerationStructure,
            BindlessHandle,
        };

        template <typename T>
        inline void EnqueueResource(EType Type, T Handle)
        {
            static_assert(sizeof(T) <= sizeof(uint64), "Vulkan resource handle type size too large.");
            EnqueueGenericResource(Type, (uint64)Handle);
        }

        void ReleaseResources(bool bDeleteImmediately = false);

        inline void Clear() { ReleaseResources(true); }

    private:
        void EnqueueGenericResource(EType Type, uint64 Handle);
        struct Entry
        {
            EType StructureType;
            uint32 FrameNumber;
            uint64 FenceCounter;
            CmdBuffer *CmdBuffer;

            uint64 Handle;
            // VulkanAllocation Allocation;
            // DeviceMemoryAllocation *DeviceMemoryAllocation;
        };
        std::vector<Entry> Entries;
    };
}

class Device
{
public:
    Device(RHI *rhi, VkPhysicalDevice Gpu);

    ~Device();

    void InitGPU();

    void CreateDevice(std::vector<const char *> &layers, std::vector<const char *> &extensions);

    void Destroy();

    void WaitUntilIdle();

    inline Queue *GetGraphicsQueue() { return gfxQueue; }

    inline Queue *GetComputeQueue() { return computeQueue; }

    inline Queue *GetTransferQueue() { return transferQueue; }

    inline Queue *GetPresentQueue() { return presentQueue; }

    inline VkPhysicalDevice GetPhysicalHandle() const { return gpu; }

    bool SupportsBindless() const;

    const VkComponentMapping &GetFormatComponentMapping(EPixelFormat UEFormat) const;

    inline VkDevice GetInstanceHandle() const { return device; }
    inline VmaAllocator GetAllocator() const { return allocator; }

    // 382
    const VkFormatProperties &GetFormatProperties(VkFormat InFormat) const;

    inline const VkPhysicalDeviceProperties &GetDeviceProperties() const { return gpuProps; }
    // 398
    inline VulkanRHI::DeferredDeletionQueue2 &GetDeferredDeletionQueue() { return deferredDeletionQueue; }
    // 403
    inline VulkanRHI::StagingManager &GetStagingManager() { return stagingManager; }
    // 423
    inline DescriptorPoolsManager &GetDescriptorPoolsManager() { return *descriptorPoolsManager; }

    // 428
    inline BindlessDescriptorManager *GetBindlessDescriptorManager() { return bindlessDescriptorManager; }

    inline CommandListContextImmediate &GetImmediateContext() { return *immediateContext; }

    // 450
    void NotifyDeletedImage(VkImage Image, bool bRenderTarget);

    inline VulkanRHI::FenceManager &GetFenceManager() { return fenceManager; }

    // 471
    void SubmitCommandsAndFlushGPU();

    inline bool SupportsParallelRendering() const { return false; }

    void SetupPresentQueue(VkSurfaceKHR Surface);

private:
    void SubmitCommands(CommandListContext *Context);

    VkDevice device;

    VulkanRHI::DeferredDeletionQueue2 deferredDeletionQueue;
    VulkanRHI::StagingManager stagingManager;
    VulkanRHI::FenceManager fenceManager;

    // Active on >= SM4
    DescriptorPoolsManager *descriptorPoolsManager = nullptr;
    BindlessDescriptorManager *bindlessDescriptorManager = nullptr;

    VkPhysicalDevice gpu;
    VkPhysicalDeviceProperties gpuProps;

    PhysicalDeviceFeatures physicalDeviceFeatures;

    std::vector<VkQueueFamilyProperties> queueFamilyProps;
    std::array<VkFormatProperties, 180> FormatProperties;

    Queue *gfxQueue;
    Queue *computeQueue;
    Queue *transferQueue;
    Queue *presentQueue;

    VkComponentMapping PixelFormatComponentMapping[PF_MAX];

    CommandListContextImmediate *immediateContext;
    CommandListContext *computeContext;
    std::vector<CommandListContext *> commandContexts;

    RHI *rhi = nullptr;
    VmaAllocator allocator{VK_NULL_HANDLE};

    void SetupFormats();
};