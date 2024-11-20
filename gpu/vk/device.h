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
#include "state.h"

#include "gpu/core/pixel_format.h"
#include "resources.h"

class Queue;
class RHI;
class CmdBuffer;
class CommandListContextImmediate;
class CommandListContext;
class DescriptorPoolsManager;
class BindlessDescriptorManager;
class Device;
class RenderPassManager;
class PipelineStateCacheManager;
class VulkanShaderFactory;

extern const std::vector<const char *> deviceExtensions;
extern const std::vector<const char *> validationLayers;

struct OptionalVulkanDeviceExtensions
{
    union
    {
        struct
        {
            // Optional Extensions

            // Vendor specific
            uint64 HasQcomRenderPassTransform : 1;

            // Promoted to 1.1

            // Promoted to 1.2
            uint64 HasKHRRenderPass2 : 1;
            uint64 HasEXTSubgroupSizeControl : 1;
            uint64 HasBufferDeviceAddress : 1;

            // Promoted to 1.3
        };
        uint64 Packed;
    };

    OptionalVulkanDeviceExtensions()
    {
        static_assert(sizeof(Packed) == sizeof(OptionalVulkanDeviceExtensions), "More bits needed for Packed!");
        Packed = 0;
    }

#if VULKAN_RHI_RAYTRACING
    inline bool HasRaytracingExtensions() const
    {
        return HasAccelerationStructure &&
               (HasRayTracingPipeline || HasRayQuery) &&
               HasEXTDescriptorIndexing &&
               HasBufferDeviceAddress &&
               HasDeferredHostOperations &&
               HasSPIRV_14 &&
               HasShaderFloatControls;
    }
#endif
};

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

        void OnCmdBufferDeleted(CmdBuffer *CmdBuffer);

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

    inline bool HasUnifiedMemory() const { return false; }

    bool SupportsBindless() const;

    const VkComponentMapping &GetFormatComponentMapping(PixelFormat UEFormat) const;

    inline VkDevice GetInstanceHandle() const { return device; }
    inline VmaAllocator GetAllocator() const { return allocator; }

    // 371
    inline const VulkanSamplerState &GetDefaultSampler() const
    {
        return *DefaultSampler;
    }

    inline const View::TextureView &GetDefaultImageView() const
    {
        return DefaultTexture->DefaultView->GetTextureView();
    }

    const VkFormatProperties &GetFormatProperties(VkFormat InFormat) const;

    inline const VkPhysicalDeviceProperties &GetDeviceProperties() const { return gpuProps; }
    inline const VkPhysicalDeviceLimits &GetLimits() const { return gpuProps.limits; }
    // 398
    inline VulkanRHI::DeferredDeletionQueue2 &GetDeferredDeletionQueue() { return deferredDeletionQueue; }
    // 403
    inline VulkanRHI::StagingManager &GetStagingManager() { return stagingManager; }
    // 413
    inline RenderPassManager &GetRenderPassManager() { return *renderPassManager; }
    // 423
    inline DescriptorPoolsManager &GetDescriptorPoolsManager() { return *descriptorPoolsManager; }

    // 428
    inline BindlessDescriptorManager *GetBindlessDescriptorManager() { return bindlessDescriptorManager; }

    inline std::unordered_map<uint32, std::shared_ptr<SamplerState>> &GetSamplerMap() { return SamplerMap; }

    // 438
    inline VulkanShaderFactory &GetShaderFactory() { return ShaderFactory; }

    inline CommandListContextImmediate &GetImmediateContext() { return *immediateContext; }

    // 450
    void NotifyDeletedImage(VkImage Image, bool bRenderTarget);

    inline VulkanRHI::FenceManager &GetFenceManager() { return fenceManager; }

    // 469
    void PrepareForCPURead();

    void SubmitCommandsAndFlushGPU();

    inline class PipelineStateCacheManager *GetPipelineStateCache() { return PipelineStateCache; }

    void NotifyDeletedGfxPipeline(class VulkanGraphicsPipelineState *Pipeline);

    inline const OptionalVulkanDeviceExtensions &GetOptionalExtensions() const { return OptionalDeviceExtensions; }

    inline bool SupportsParallelRendering() const { return false; }

    void SetupPresentQueue(VkSurfaceKHR Surface);

private:
    void SubmitCommands(CommandListContext *Context);

    VkDevice device;

    VulkanRHI::DeferredDeletionQueue2 deferredDeletionQueue;
    VulkanRHI::StagingManager stagingManager;
    VulkanRHI::FenceManager fenceManager;
    RenderPassManager *renderPassManager = nullptr;

    // Active on >= SM4
    DescriptorPoolsManager *descriptorPoolsManager = nullptr;
    BindlessDescriptorManager *bindlessDescriptorManager = nullptr;

    VulkanShaderFactory ShaderFactory;

    std::shared_ptr<VulkanSamplerState> DefaultSampler;
    VulkanTexture *DefaultTexture;

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

    std::unordered_map<uint32, std::shared_ptr<SamplerState>> SamplerMap;

    CommandListContextImmediate *immediateContext;
    CommandListContext *computeContext;
    std::vector<CommandListContext *> commandContexts;

    RHI *rhi = nullptr;
    VmaAllocator allocator{VK_NULL_HANDLE};

    OptionalVulkanDeviceExtensions OptionalDeviceExtensions;

    void SetupFormats();

    PipelineStateCacheManager *PipelineStateCache;

    friend class RHI;
    friend class VulkanGraphicsPipelineState;
};