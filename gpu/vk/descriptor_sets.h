#pragma once
#include <map>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <list>

#include "device.h"
#include "../definitions.h"
#include "Volk/volk.h"
#include "gpu/RHI/RHIDefinitions.h"
#include "vulkan_memory.h"
#include "gpu/core/misc/crc.h"

namespace VulkanRHI
{
    class DeviceChild;
}

int memcmp(const void *a, const void *b, int length);

namespace std
{
    template <>
    struct hash<VkDescriptorType>
    {
        size_t operator()(const VkDescriptorType &x) const
        {
            return static_cast<size_t>(x);
        }
    };
}

// 49
/// Information for remapping descriptor sets when combining layouts
struct DescriptorSetRemappingInfo
{
    inline bool operator==(const DescriptorSetRemappingInfo &In) const { return true; }
    inline bool operator!=(const DescriptorSetRemappingInfo &In) const
    {
        return !(*this == In);
    }
};

// 781
//  Smaller data structure for runtime information about descriptor sets and bindings for a pipeline
class VulkanGfxPipelineDescriptorInfo
{
public:
    inline bool IsInitialized() const { return bInitialized; }

protected:
    bool bInitialized;
};

struct UniformBufferGatherInfo
{
    UniformBufferGatherInfo()
    {
        memset(CodeHeaders, 0, sizeof(ShaderHeader *) * ShaderStage::NumStages);
    }

    // These maps are used to find UBs that are used on multiple stages
    std::unordered_map<uint32, VkShaderStageFlags> UBLayoutsToUsedStageMap;
    std::unordered_map<uint32, VkShaderStageFlags> CommonUBLayoutsToStageMap;

    const ShaderHeader *CodeHeaders[ShaderStage::NumStages];
};

// Information for the layout of descriptor sets; does not hold runtime objects
class DescriptorSetsLayoutInfo
{
public:
    DescriptorSetsLayoutInfo()
    {
        // Add expected descriptor types
        for (uint32_t i = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; i <= VK_DESCRIPTOR_TYPE_END_RANGE; ++i)
        {
            layoutTypes.insert(std::pair(static_cast<VkDescriptorType>(i), 0));
        }
    }

    inline uint32_t GetTypesUsed(VkDescriptorType Type) const
    {
        if (layoutTypes.count(Type))
        {
            return layoutTypes.at(Type);
        }
        else
        {
            return 0;
        }
    }

    struct SetLayout
    {
        std::vector<VkDescriptorSetLayoutBinding> LayoutBindings;
        uint32_t Hash;

        inline void GenerateHash()
        {
            Hash = MemCrc32(LayoutBindings.data(), sizeof(VkDescriptorSetLayoutBinding) * LayoutBindings.size());
        }

        friend uint32_t GetTypeHash(const SetLayout &In)
        {
            return In.Hash;
        }

        inline bool operator==(const SetLayout &In) const
        {
            if (In.Hash != Hash)
            {
                return false;
            }

            const int32_t NumBindings = LayoutBindings.size();
            if (In.LayoutBindings.size() != NumBindings)
            {
                return false;
            }

            if (NumBindings != 0 && memcmp(In.LayoutBindings.data(), LayoutBindings.data(), NumBindings * sizeof(VkDescriptorSetLayoutBinding)) != 0)
            {
                return false;
            }

            return true;
        }

        inline bool operator!=(const SetLayout &In) const
        {
            return !(*this == In);
        }
    };

    void ProcessBindingsForStage(VkShaderStageFlagBits StageFlags, ShaderStage::EStage DescSetStage,
                                 const ShaderHeader &CodeHeader, UniformBufferGatherInfo &OutUBGatherInfo) const;

    const std::vector<SetLayout> &GetLayouts() const { return setLayouts; }

    template <bool bIsCompute>
    void FinalizeBindings(const Device &device, const UniformBufferGatherInfo &UBGatherInfo, const std::vector<SamplerState *> &ImmutableSamplers);

    /// UE的哈希表容器是通过GetTypeHash获得散列值的。在UE中，想要作为TMap的键，就必须支持这个函数。
    friend uint32 GetTypeHash(const DescriptorSetsLayoutInfo &In) { return In.hash; }

    inline bool operator==(const DescriptorSetsLayoutInfo &In) const
    {
        if (In.hash != hash)
        {
            return false;
        }

        if (In.BindPoint != BindPoint)
        {
            return false;
        }

        if (In.setLayouts.size() != setLayouts.size())
        {
            return false;
        }

        if (In.typesUsageID != typesUsageID)
        {
            return false;
        }

        for (int32 Index = 0; Index < In.setLayouts.size(); ++Index)
        {
            if (In.setLayouts[Index] != setLayouts[Index])
            {
                return false;
            }
        }

        if (RemappingInfo != In.RemappingInfo)
        {
            return false;
        }

        return true;
    }

    void CopyFrom(const DescriptorSetsLayoutInfo &Info)
    {
        layoutTypes = Info.layoutTypes;
        hash = Info.hash;
        typesUsageID = Info.typesUsageID;
        setLayouts = Info.setLayouts;
        RemappingInfo = Info.RemappingInfo;
    }

    inline bool HasInputAttachments() const
    {
        return GetTypesUsed(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) > 0;
    }

protected:
    // 每种类型descriptor各使用了多少个
    std::unordered_map<VkDescriptorType, uint32> layoutTypes;
    std::vector<SetLayout> setLayouts;
    uint32_t hash = 0;
    uint32_t typesUsageID = ~0;
    void CompileTypesUsageID();
    VkPipelineBindPoint BindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
    DescriptorSetRemappingInfo RemappingInfo;
};

namespace std
{
    template <>
    struct hash<DescriptorSetsLayoutInfo>
    {
        size_t operator()(const DescriptorSetsLayoutInfo &v) const
        {
            return GetTypeHash(v);
        }
    };

    template <>
    struct hash<DescriptorSetsLayoutInfo::SetLayout>
    {
        size_t operator()(const DescriptorSetsLayoutInfo::SetLayout &v) const
        {
            return GetTypeHash(v);
        }
    };
}

// 443
struct DescriptorSetLayoutEntry
{
    VkDescriptorSetLayout Handle = 0;
    uint32 HandleId = 0;
};

using DescriptorSetLayoutMap = std::unordered_map<DescriptorSetsLayoutInfo::SetLayout, DescriptorSetLayoutEntry>;

// The actual run-time descriptor set layouts
class DescriptorSetsLayout : public DescriptorSetsLayoutInfo
{
public:
    DescriptorSetsLayout(Device *InDevice);
    ~DescriptorSetsLayout();

    // Can be called only once, the idea is that the Layout remains fixed.
    void Compile(DescriptorSetLayoutMap &DSetLayoutMap);

    inline const std::vector<VkDescriptorSetLayout> &GetHandles() const { return layoutHandles; }

private:
    Device *device;
    std::vector<VkDescriptorSetLayout> layoutHandles;
    std::vector<uint32_t> layoutHandleIds;
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;

    VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo;
};

class DescriptorPool
{
public:
    DescriptorPool(Device *InDevice, const DescriptorSetsLayout &Layout, uint32_t MaxSetsAllocations);
    ~DescriptorPool();

    inline VkDescriptorPool GetHandle() const
    {
        return descriptorPool;
    }
    void Reset();

private:
    Device *device;

    uint32_t MaxDescriptorSets;
    uint32_t NumAllocatedDescriptorSets;
    uint32_t PeakAllocatedDescriptorSets;

    // Tracks number of allocated types, to ensure that we are not exceeding our allocated limit
    const DescriptorSetsLayout &layout;
    VkDescriptorPool descriptorPool;

    friend class CommandListContext;
};

class TypedDescriptorPoolSet
{
    typedef TList<DescriptorPool *> PoolList;
    DescriptorPool *PushNewPool();

protected:
    friend class DescriptorPoolSetContainer;
    TypedDescriptorPoolSet(Device *InDevice, const DescriptorSetsLayout &InLayout)
        : device(InDevice), layout(InLayout), poolsCount(0)
    {
        PushNewPool();
    };
    ~TypedDescriptorPoolSet();
    void Reset();

private:
    Device *device;
    const DescriptorSetsLayout &layout;
    uint32_t poolsCount;

    PoolList *poolListHead = nullptr;
    PoolList *poolListCurrent = nullptr;
};

class DescriptorPoolSetContainer
{
public:
    DescriptorPoolSetContainer(Device *InDevice)
        : device(InDevice), LastFrameUsed(GFrameNumberRenderThread), bUsed(true)
    {
    }

    void Reset();

    inline void SetUsed(bool bInUsed)
    {
        bUsed = bInUsed;
        LastFrameUsed = bUsed ? GFrameNumberRenderThread : LastFrameUsed;
    }

    inline bool IsUnused() const
    {
        return !bUsed;
    }

private:
    Device *device;
    std::map<uint32_t, TypedDescriptorPoolSet *> TypedDescriptorPools;

    uint32_t LastFrameUsed;
    bool bUsed;
};

class DescriptorPoolsManager
{
public:
    ~DescriptorPoolsManager();
    void Init(Device *InDevice)
    {
        device = InDevice;
    }
    DescriptorPoolSetContainer &AcquirePoolSetContainer();
    void ReleasePoolSet(DescriptorPoolSetContainer &PoolSet);

private:
    Device *device = nullptr;
    std::vector<DescriptorPoolSetContainer *> PoolSets;
};

// Manager for resource descriptors used in bindless rendering.
class BindlessDescriptorManager : public VulkanRHI::DeviceChild
{
public:
    BindlessDescriptorManager(Device *InDevice);

    inline VkPipelineLayout GetPipelineLayout() const { return BindlessPipelineLayout; }

    RHIDescriptorHandle ReserveDescriptor(VkDescriptorType DescriptorType);

    void UpdateImage(RHIDescriptorHandle DescriptorHandle, VkImageView VulkanImage, bool bIsDepthStencil, bool bImmediateUpdate = true);

private:
    const bool bIsSupported = false;

    VkPipelineLayout BindlessPipelineLayout = VK_NULL_HANDLE;
};

// 1278
//  Layout for a Pipeline, also includes DescriptorSets layout
class VulkanLayout : public VulkanRHI::DeviceChild
{
public:
    VulkanLayout(Device *InDevice);
    virtual ~VulkanLayout();

    virtual bool IsGfxLayout() const = 0;

    inline const DescriptorSetsLayout &GetDescriptorSetsLayout() const { return descriptorSetsLayout; }

    inline VkPipelineLayout GetPipelineLayout() const
    {
        return device->SupportsBindless() ? device->GetBindlessDescriptorManager()->GetPipelineLayout() : PipelineLayout;
    }

    // 	inline bool HasDescriptors() const
    // 	{
    // 		return DescriptorSetLayout.GetLayouts().Num() > 0;
    // 	}

    inline uint32 GetDescriptorSetLayoutHash() const
    {
        printf("Have not implement %s %d\n", __FILE__, __LINE__);
        return -1;
        // return DescriptorSetLayout.GetHash();
    }

    // 	void PatchSpirvBindings(FVulkanShader::FSpirvCode& SpirvCode, EShaderFrequency Frequency, const FVulkanShaderHeader& CodeHeader) const;

protected:
    DescriptorSetsLayout descriptorSetsLayout;
    VkPipelineLayout PipelineLayout;

    // 	template <bool bIsCompute>
    // 	inline void FinalizeBindings(const FUniformBufferGatherInfo& UBGatherInfo)
    // 	{
    // 		// Setting descriptor is only allowed prior to compiling the layout
    // 		check(DescriptorSetLayout.GetHandles().Num() == 0);

    // 		DescriptorSetLayout.FinalizeBindings<bIsCompute>(UBGatherInfo);
    // 	}

    // 	inline void ProcessBindingsForStage(VkShaderStageFlagBits StageFlags, ShaderStage::EStage DescSet, const FVulkanShaderHeader& CodeHeader, FUniformBufferGatherInfo& OutUBGatherInfo) const
    // 	{
    // 		// Setting descriptor is only allowed prior to compiling the layout
    // 		check(DescriptorSetLayout.GetHandles().Num() == 0);

    // 		DescriptorSetLayout.ProcessBindingsForStage(StageFlags, DescSet, CodeHeader, OutUBGatherInfo);
    // 	}

    void Compile(DescriptorSetLayoutMap &DSetLayoutMap);

    // 	friend class FVulkanComputePipeline;
    // 	friend class FVulkanGfxPipeline;
    friend class PipelineStateCacheManager;
#if VULKAN_RHI_RAYTRACING
    friend class FVulkanRayTracingPipelineState;
#endif
};

class VulkanGfxLayout : public VulkanLayout
{
public:
    VulkanGfxLayout(Device *InDevice) : VulkanLayout(InDevice) {}

    virtual bool IsGfxLayout() const final override { return true; }

    inline const VulkanGfxPipelineDescriptorInfo &GetGfxPipelineDescriptorInfo() const { return GfxPipelineDescriptorInfo; }

    // 	bool UsesInputAttachment(FVulkanShaderHeader::EAttachmentType AttachmentType) const;

protected:
    VulkanGfxPipelineDescriptorInfo GfxPipelineDescriptorInfo;
    friend class PipelineStateCacheManager;
};
