#pragma once
#include <cstdint>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

#include "definitions.h"
#include "Volk/volk.h"
#include "device.h"
#include "RHI/RHIDefinitions.h"
#include "core/misc/crc.h"
#include "vulkan_memory.h"
#include "util.h"
#include "private.h"
#include "configuration.h"

extern uint32 GFrameNumberRenderThread;

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
} // namespace std

/// @brief 给DescriptorSetRemappingInfo用
struct InputAttachmentData
{
    uint16 BindingIndex = UINT16_MAX;
    uint8 DescriptorSet = UINT8_MAX;
    ShaderHeader::AttachmentType Type = ShaderHeader::AttachmentType::Count;
};

// 49
/// Information for remapping descriptor sets when combining layouts
/// @remark 为什么需要这个？
struct DescriptorSetRemappingInfo
{
    struct RemappingInfo
    {
        uint16 NewDescriptorSet = UINT16_MAX;
        uint16 NewBindingIndex = UINT16_MAX;
    };

    struct UBRemappingInfo
    {
        // Remapping is only valid if there is constant data
        RemappingInfo Remapping;

        bool bHasConstantData = false;

        bool bPadding =
            false; // padding is need on memcmp/MemCrc to make sure mem align
    };

    /// @brief 存储了本set中descriptor的数量，以及各个descriptor的类型
    struct SetInfo
    {
        std::vector<VkDescriptorType> Types;
        uint16 NumImageInfos = 0;
        uint16 NumBufferInfos = 0;
#if VULKAN_RHI_RAYTRACING
        uint8 NumAccelerationStructures = 0;
#endif // VULKAN_RHI_RAYTRACING
    };
    std::vector<SetInfo> SetInfos;

    struct StageInfo
    {
        std::vector<RemappingInfo> Globals;
        std::vector<UBRemappingInfo> UniformBuffers;
        std::vector<uint16> PackedUBBindingIndices;
        uint16 PackedUBDescriptorSet = UINT16_MAX;
        uint16 Pad0 = 0;

        inline bool IsEmpty() const
        {
            if (Globals.size() != 0)
            {
                return false;
            }

            if (UniformBuffers.size() != 0)
            {
                return false;
            }

            if (PackedUBBindingIndices.size() != 0)
            {
                return false;
            }

            return true;
        }
    };
    std::array<StageInfo, ShaderStage::NumStages> StageInfos;
    std::vector<InputAttachmentData> InputAttachmentData;

    inline bool IsEmpty() const
    {
        if (SetInfos.size() == 0)
        {
            for (int32 Index = 0; Index < ShaderStage::NumStages; ++Index)
            {
                if (!StageInfos[Index].IsEmpty())
                {
                    return false;
                }
            }

            return true;
        }

        return false;
    }

    uint32 AddGlobal(uint32 Stage, int32 GlobalIndex, uint32 NewDescriptorSet,
                     VkDescriptorType InType, uint16 CombinedSamplerStateAlias)
    {
        // Combined Image Samplers point both the texture and the sampler to the
        // same descriptor
        check(CombinedSamplerStateAlias == UINT16_MAX || InType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        uint32 NewBindingIndex;
        if (CombinedSamplerStateAlias == UINT16_MAX)
        {
            NewBindingIndex = SetInfos[NewDescriptorSet].Types.size();
            SetInfos[NewDescriptorSet].Types.push_back(InType);
        }
        else
        {
            NewBindingIndex = StageInfos[Stage].Globals[CombinedSamplerStateAlias].NewBindingIndex;
        }

        int32 RemappingIndex = StageInfos[Stage].Globals.size();
        StageInfos[Stage].Globals.push_back({});
        check(RemappingIndex == GlobalIndex);
        DescriptorSetRemappingInfo::RemappingInfo &Remapping = StageInfos[Stage].Globals[RemappingIndex];
        Remapping.NewDescriptorSet = NewDescriptorSet;
        Remapping.NewBindingIndex = NewBindingIndex;

        return NewBindingIndex;
    }

    DescriptorSetRemappingInfo::UBRemappingInfo AddUBWithData(
        uint32 Stage, int32 UniformBufferIndex, uint32 NewDescriptorSet,
        VkDescriptorType InType, uint32 &OutNewBindingIndex)
    {
        OutNewBindingIndex = SetInfos[NewDescriptorSet].Types.size();
        SetInfos[NewDescriptorSet].Types.push_back(InType);

        int32 UBRemappingIndex = StageInfos[Stage].UniformBuffers.size();
        StageInfos[Stage].UniformBuffers.push_back(UBRemappingInfo());
        check(UBRemappingIndex == UniformBufferIndex);
        DescriptorSetRemappingInfo::UBRemappingInfo &UBRemapping =
            StageInfos[Stage].UniformBuffers[UBRemappingIndex];
        UBRemapping.bHasConstantData = true;
        UBRemapping.Remapping.NewDescriptorSet = NewDescriptorSet;
        UBRemapping.Remapping.NewBindingIndex = OutNewBindingIndex;

        return UBRemapping;
    }

    inline bool operator==(const DescriptorSetRemappingInfo &In) const
    {
        printf("fuck\n");
        return true;
    }
    inline bool operator!=(const DescriptorSetRemappingInfo &In) const
    {
        return !(*this == In);
    }

    void AddUBResourceOnly(uint32 Stage, int32 UniformBufferIndex)
    {
        int32 UBRemappingIndex = StageInfos[Stage].UniformBuffers.size();
        StageInfos[Stage].UniformBuffers.push_back({});
        check(UBRemappingIndex == UniformBufferIndex);
        DescriptorSetRemappingInfo::UBRemappingInfo &UBRemapping =
            StageInfos[Stage].UniformBuffers[UBRemappingIndex];
        UBRemapping.bHasConstantData = false;
    }
};

// Smaller data structure for runtime information about descriptor sets and bindings for a pipeline
class VulkanComputePipelineDescriptorInfo
{
public:
    VulkanComputePipelineDescriptorInfo()
        : HasDescriptorsInSetMask(0), RemappingInfo(nullptr), bInitialized(false) {}

    inline bool GetDescriptorSetAndBindingIndex(const ShaderHeader::Type Type, int32 ParameterIndex,
                                                uint8 &OutDescriptorSet, uint32 &OutBindingIndex) const
    {
        switch (Type)
        {
        case ShaderHeader::UniformBuffer:
            ensure(RemappingUBInfos[ParameterIndex]->bHasConstantData);
            OutDescriptorSet = RemappingUBInfos[ParameterIndex]->Remapping.NewDescriptorSet;
            OutBindingIndex = RemappingUBInfos[ParameterIndex]->Remapping.NewBindingIndex;
            break;
        case ShaderHeader::Global:
            OutDescriptorSet = RemappingGlobalInfos[ParameterIndex]->NewDescriptorSet;
            OutBindingIndex = RemappingGlobalInfos[ParameterIndex]->NewBindingIndex;
            break;
        default:
            check(0);
            return false;
        }
        return true;
    }

    inline const std::vector<DescriptorSetRemappingInfo::RemappingInfo> &GetGlobalRemappingInfo() const
    {
        return RemappingInfo->StageInfos[0].Globals;
    }

    inline VkDescriptorType GetDescriptorType(uint8 DescriptorSet, int32 DescriptorIndex) const
    {
        return RemappingInfo->SetInfos[DescriptorSet].Types[DescriptorIndex];
    }

    inline bool IsInitialized() const { return bInitialized; }

    void Initialize(const DescriptorSetRemappingInfo &InRemappingInfo);

protected:
    // Cached data from FDescriptorSetRemappingInfo
    std::vector<const DescriptorSetRemappingInfo::UBRemappingInfo *> RemappingUBInfos;
    std::vector<const DescriptorSetRemappingInfo::RemappingInfo *> RemappingGlobalInfos;
    std::vector<const uint16 *> RemappingPackedUBInfos;
    uint32 HasDescriptorsInSetMask;
    const DescriptorSetRemappingInfo *RemappingInfo;
    bool bInitialized;

    friend class ComputePipelineDescriptorState;
};

// 781
//  Smaller data structure for runtime information about descriptor sets and
//  bindings for a pipeline
class VulkanGfxPipelineDescriptorInfo
{
public:
    VulkanGfxPipelineDescriptorInfo() : HasDescriptorsInSetMask(0), RemappingInfo(nullptr), bInitialized(false) {}

    inline bool GetDescriptorSetAndBindingIndex(const ShaderHeader::Type Type, const ShaderStage::Stage Stage,
                                                int32 ParameterIndex, uint8 &OutDescriptorSet, uint32 &OutBindingIndex) const
    {
        switch (Type)
        {
        case ShaderHeader::UniformBuffer:
            ensure(RemappingUBInfos[Stage][ParameterIndex]->bHasConstantData);
            OutDescriptorSet = RemappingUBInfos[Stage][ParameterIndex]->Remapping.NewDescriptorSet;
            OutBindingIndex = RemappingUBInfos[Stage][ParameterIndex]->Remapping.NewBindingIndex;
            break;
        case ShaderHeader::Global:
            OutDescriptorSet = RemappingGlobalInfos[Stage][ParameterIndex]->NewDescriptorSet;
            OutBindingIndex = RemappingGlobalInfos[Stage][ParameterIndex]->NewBindingIndex;
            break;
        default:
            check(0);
            return false;
        }
        return true;
    }

    inline const std::vector<DescriptorSetRemappingInfo::RemappingInfo> &GetGlobalRemappingInfo(ShaderStage::Stage stage) const
    {
        return RemappingInfo->StageInfos[stage].Globals;
    }

    inline VkDescriptorType GetDescriptorType(uint8 DescriptorSet, int32 DescriptorIndex) const
    {
        return RemappingInfo->SetInfos[DescriptorSet].Types[DescriptorIndex];
    }

    inline bool IsInitialized() const { return bInitialized; }

    void Initialize(const DescriptorSetRemappingInfo &InRemappingInfo);

protected:
    // Cached data from DescriptorSetRemappingInfo
    std::vector<const DescriptorSetRemappingInfo::UBRemappingInfo *> RemappingUBInfos[ShaderStage::NumStages];
    std::vector<const DescriptorSetRemappingInfo::RemappingInfo *> RemappingGlobalInfos[ShaderStage::NumStages];
    std::vector<const uint16 *> RemappingPackedUBInfos[ShaderStage::NumStages];
    uint32 HasDescriptorsInSetMask;

    const DescriptorSetRemappingInfo *RemappingInfo;
    bool bInitialized = true;

    friend class GraphicsPipelineDescriptorState;
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
        for (uint32_t i = VK_DESCRIPTOR_TYPE_BEGIN_RANGE;
             i <= VK_DESCRIPTOR_TYPE_END_RANGE; ++i)
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
            Hash =
                MemCrc32(LayoutBindings.data(), sizeof(VkDescriptorSetLayoutBinding) *
                                                    LayoutBindings.size());
        }

        friend uint32_t GetTypeHash(const SetLayout &In) { return In.Hash; }

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

            if (NumBindings != 0 &&
                memcmp(In.LayoutBindings.data(), LayoutBindings.data(),
                       NumBindings * sizeof(VkDescriptorSetLayoutBinding)) != 0)
            {
                return false;
            }

            return true;
        }

        inline bool operator!=(const SetLayout &In) const { return !(*this == In); }
    };

    void ProcessBindingsForStage(VkShaderStageFlagBits StageFlags,
                                 ShaderStage::Stage DescSetStage,
                                 const ShaderHeader &CodeHeader,
                                 UniformBufferGatherInfo &OutUBGatherInfo) const;

    const std::vector<SetLayout> &GetLayouts() const { return setLayouts; }

    template <bool bIsCompute>
    void FinalizeBindings(const Device &device,
                          const UniformBufferGatherInfo &UBGatherInfo,
                          const std::vector<SamplerState *> &ImmutableSamplers);

    void GenerateHash(const std::vector<SamplerState *> &ImmutableSamplers);

    /// UE的哈希表容器是通过GetTypeHash获得散列值的。在UE中，想要作为TMap的键，就必须支持这个函数。
    friend uint32 GetTypeHash(const DescriptorSetsLayoutInfo &In)
    {
        return In.hash;
    }

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

    inline uint32 GetTypesUsageID() const { return typesUsageID; }

    inline bool HasInputAttachments() const
    {
        return GetTypesUsed(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) > 0;
    }

protected:
    /// 统计整个管线中每种类型descriptor各使用了多少个
    std::unordered_map<VkDescriptorType, uint32> layoutTypes;
    std::vector<SetLayout> setLayouts;
    uint32_t hash = 0;
    uint32_t typesUsageID = ~0;
    void CompileTypesUsageID();
    void AddDescriptor(int32 DescriptorSetIndex,
                       const VkDescriptorSetLayoutBinding &Descriptor);
    VkPipelineBindPoint BindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
    DescriptorSetRemappingInfo RemappingInfo;

    friend class PipelineStateCacheManager;
    friend class CommonPipelineDescriptorState;
    friend class VulkanPipelineLayout;
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
} // namespace std

// 443
struct DescriptorSetLayoutEntry
{
    VkDescriptorSetLayout Handle = 0;
    uint32 HandleId = 0;
};

// 670
union HashableDescriptorInfo
{
    struct
    {
        uint32 Max0;
        uint32 Max1;
        uint32 LayoutId;
    } Layout;
    struct
    {
        uint32 Id;
        uint32 Offset;
        uint32 Range;
    } Buffer;
    struct
    {
        uint32 SamplerId;
        uint32 ImageViewId;
        uint32 ImageLayout;
    } Image;
    struct
    {
        uint32 Id;
        uint32 Zero1;
        uint32 Zero2;
    } BufferView;
};

///  This container holds the actual VkWriteDescriptorSet structures; a Compute pipeline uses the arrays 'as-is', whereas a
///  Gfx PSO will have one big array and chunk it depending on the stage (eg Vertex, Pixel).
struct DescriptorSetWriteContainer
{
    std::vector<HashableDescriptorInfo> HashableDescriptorInfo;
    std::vector<VkDescriptorImageInfo> DescriptorImageInfo;
    std::vector<VkDescriptorBufferInfo> DescriptorBufferInfo;
    std::vector<VkWriteDescriptorSet> DescriptorWrites;
#if VULKAN_RHI_RAYTRACING
    TArray<VkAccelerationStructureKHR> AccelerationStructures;
    TArray<VkWriteDescriptorSetAccelerationStructureKHR> AccelerationStructureWrites;
#endif // VULKAN_RHI_RAYTRACING
    std::vector<uint8> BindingToDynamicOffsetMap;
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
    inline const std::vector<uint32> &GetHandleIds() const { return layoutHandleIds; }
    inline const VkDescriptorSetAllocateInfo &GetAllocateInfo() const { return DescriptorSetAllocateInfo; }

    inline uint32 GetHash() const
    {
        check(hash != 0);
        return hash;
    }

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
    DescriptorPool(Device *InDevice, const DescriptorSetsLayout &Layout,
                   uint32_t MaxSetsAllocations);
    ~DescriptorPool();

    inline VkDescriptorPool GetHandle() const { return descriptorPool; }
    void Reset();
    bool AllocateDescriptorSets(const VkDescriptorSetAllocateInfo &InDescriptorSetAllocateInfo, VkDescriptorSet *OutSets);

private:
    Device *device;

    uint32_t MaxDescriptorSets;
    uint32_t NumAllocatedDescriptorSets;
    uint32_t PeakAllocatedDescriptorSets;

    // Tracks number of allocated types, to ensure that we are not exceeding our
    // allocated limit
    const DescriptorSetsLayout &layout;
    VkDescriptorPool descriptorPool;

    friend class CommandListContext;
};

class TypedDescriptorPoolSet
{
    typedef TList<DescriptorPool *> PoolList;
    DescriptorPool *GetFreePool(bool bForceNewPool = false);
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

public:
    bool AllocateDescriptorSets(const DescriptorSetsLayout &Layout, VkDescriptorSet *OutSets);

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
        : device(InDevice),
          LastFrameUsed(GFrameNumberRenderThread),
          bUsed(true) {}

    ~DescriptorPoolSetContainer();

    TypedDescriptorPoolSet *AcquireTypedPoolSet(const DescriptorSetsLayout &Layout);

    void Reset();

    inline void SetUsed(bool bInUsed)
    {
        bUsed = bInUsed;
        LastFrameUsed = bUsed ? GFrameNumberRenderThread : LastFrameUsed;
    }

    inline bool IsUnused() const { return !bUsed; }

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
    void Init(Device *InDevice) { device = InDevice; }
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

    inline VkPipelineLayout GetPipelineLayout() const
    {
        return BindlessPipelineLayout;
    }

    RHIDescriptorHandle ReserveDescriptor(VkDescriptorType DescriptorType);

    void UpdateImage(RHIDescriptorHandle DescriptorHandle,
                     VkImageView VulkanImage, bool bIsDepthStencil,
                     bool bImmediateUpdate = true);

private:
    const bool bIsSupported = false;

    VkPipelineLayout BindlessPipelineLayout = VK_NULL_HANDLE;
};

// 855

/// This class encapsulates updating VkWriteDescriptorSet structures (but doesn't own them), and their flags for dirty ranges;
/// it is intended to be used to access a sub-region of a long array of VkWriteDescriptorSet
/// (ie FVulkanDescriptorSetWriteContainer)
class DescriptorSetWriter
{
public:
    DescriptorSetWriter()
        : WriteDescriptors(nullptr), BindingToDynamicOffsetMap(nullptr),
          DynamicOffsets(nullptr), NumWrites(0), HashableDescriptorInfos(nullptr) //, bIsKeyDirty(true)
    {
    }

    bool WriteUniformBuffer(uint32 DescriptorIndex, VkBuffer BufferHandle, uint32 HandleId, VkDeviceSize Offset, VkDeviceSize Range)
    {
        return WriteBuffer<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER>(DescriptorIndex, BufferHandle, HandleId, Offset, Range);
    }

    bool WriteSampler(uint32 DescriptorIndex, const VulkanSamplerState &Sampler)
    {
        check(DescriptorIndex < NumWrites);
        check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER || WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        VkDescriptorImageInfo *ImageInfo = const_cast<VkDescriptorImageInfo *>(WriteDescriptors[DescriptorIndex].pImageInfo);
        check(ImageInfo);

        bool bChanged = false;
        if (UseVulkanDescriptorCache())
        {
            check(0);
        }
        else
        {
            bChanged = CopyAndReturnNotEqual(ImageInfo->sampler, Sampler.sampler);
        }

        return bChanged;
    }

    bool WriteImage(uint32 DescriptorIndex, const View::TextureView &textureView, VkImageLayout Layout)
    {
        return WriteTextureView<VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE>(DescriptorIndex, textureView, Layout);
    }

    bool WriteStorageImage(uint32 DescriptorIndex, const View::TextureView &TextureView, VkImageLayout Layout)
    {
        return WriteTextureView<VK_DESCRIPTOR_TYPE_STORAGE_IMAGE>(DescriptorIndex, TextureView, Layout);
    }

protected:
    void SetDescriptorSet(VkDescriptorSet DescriptorSet)
    {
        for (uint32 Index = 0; Index < NumWrites; ++Index)
        {
            WriteDescriptors[Index].dstSet = DescriptorSet;
        }
    }

    template <VkDescriptorType DescriptorType>
    bool WriteBuffer(uint32 DescriptorIndex, VkBuffer BufferHandle, uint32 HandleId, VkDeviceSize Offset,
                     VkDeviceSize Range, uint32 DynamicOffset = 0)
    {
        check(DescriptorIndex < NumWrites);
        SetWritten(DescriptorIndex);
        if (DescriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        {
            check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
        }
        else
        {
            check(WriteDescriptors[DescriptorIndex].descriptorType == DescriptorType);
        }
        VkDescriptorBufferInfo *BufferInfo = const_cast<VkDescriptorBufferInfo *>(WriteDescriptors[DescriptorIndex].pBufferInfo);
        check(BufferInfo);

        bool bChanged = false;
        if (UseVulkanDescriptorCache())
        {
            check(0);
        }
        else
        {
            bChanged = CopyAndReturnNotEqual(BufferInfo->buffer, BufferHandle);
            bChanged |= CopyAndReturnNotEqual(BufferInfo->offset, Offset);
            bChanged |= CopyAndReturnNotEqual(BufferInfo->range, Range);
        }

        if (DescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        {
            const uint8 DynamicOffsetIndex = BindingToDynamicOffsetMap[DescriptorIndex];
            DynamicOffsets[DynamicOffsetIndex] = DynamicOffset;
        }
        return bChanged;
    }

    template <VkDescriptorType DescriptorType>
    bool WriteTextureView(uint32 DescriptorIndex, const View::TextureView &TextureView, VkImageLayout Layout)
    {
        check(DescriptorIndex < NumWrites);
        SetWritten(DescriptorIndex);
        if (DescriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
        {
            checkf(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   "DescriptorType mismatch at index %d: called WriteTextureView<%d> and was expecting %d.",
                   DescriptorIndex, (uint32)DescriptorType, (uint32)WriteDescriptors[DescriptorIndex].descriptorType);

            ensureMsgf(Layout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL ||
                           Layout == VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR ||
                           Layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL ||
                           Layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL ||
                           Layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ||
                           Layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
                           Layout == VK_IMAGE_LAYOUT_GENERAL,
                       "Invalid Layout %s, Index %d, Type %s\n",
                       VK_TYPE_TO_STRING(VkImageLayout, Layout), DescriptorIndex, VK_TYPE_TO_STRING(VkDescriptorType, WriteDescriptors[DescriptorIndex].descriptorType));
        }
        else
        {
            checkf(WriteDescriptors[DescriptorIndex].descriptorType == DescriptorType,
                   "DescriptorType mismatch at index %d: called WriteTextureView<%s> and was expecting %s.",
                   DescriptorIndex, VK_TYPE_TO_STRING(VkDescriptorType, DescriptorType), VK_TYPE_TO_STRING(VkDescriptorType, WriteDescriptors[DescriptorIndex].descriptorType));
        }
        VkDescriptorImageInfo *ImageInfo = const_cast<VkDescriptorImageInfo *>(WriteDescriptors[DescriptorIndex].pImageInfo);
        check(ImageInfo);

        bool bChanged = false;
        if (UseVulkanDescriptorCache())
        {
            check(0);
        }
        else
        {
            bChanged = CopyAndReturnNotEqual(ImageInfo->imageView, TextureView.View);
            bChanged |= CopyAndReturnNotEqual(ImageInfo->imageLayout, Layout);
        }

        return bChanged;
    }

    // A view into someone else's descriptors
    VkWriteDescriptorSet *WriteDescriptors;

    // A view into the mapping from binding index to dynamic uniform buffer offsets
    uint8 *BindingToDynamicOffsetMap;

    // A view into someone else's dynamic uniform buffer offsets
    uint32 *DynamicOffsets;

    uint32 NumWrites;

    HashableDescriptorInfo *HashableDescriptorInfos;

    bool bHasVolatileResources = false;

    uint32 SetupDescriptorWrites(const std::vector<VkDescriptorType> &Types,
                                 HashableDescriptorInfo *InHashableDescriptorInfos,
                                 VkWriteDescriptorSet *InWriteDescriptors, VkDescriptorImageInfo *InImageInfo,
                                 VkDescriptorBufferInfo *InBufferInfo, uint8 *InBindingToDynamicOffsetMap,
#if VULKAN_RHI_RAYTRACING
                                 VkWriteDescriptorSetAccelerationStructureKHR *InAccelerationStructuresWriteDescriptors,
                                 VkAccelerationStructureKHR *InAccelerationStructures,
#endif // VULKAN_RHI_RAYTRACING
                                 const VulkanSamplerState &DefaultSampler, const View::TextureView &DefaultImageView);

    friend class CommonPipelineDescriptorState;
    friend class ComputePipelineDescriptorState;
    friend class GraphicsPipelineDescriptorState;
    friend class DescriptorSetCache;

    void Reset();
    void SetWritten(uint32 DescriptorIndex);
    void SetWrittenBase(uint32 DescriptorIndex);
    void InitWrittenMasks(uint32 NumDescriptorWrites);
};

// 1278
//  Layout for a Pipeline, also includes DescriptorSets layout
class VulkanPipelineLayout : public VulkanRHI::DeviceChild
{
public:
    VulkanPipelineLayout(Device *InDevice);
    virtual ~VulkanPipelineLayout();

    virtual bool IsGfxLayout() const = 0;

    inline const DescriptorSetsLayout &GetDescriptorSetsLayout() const
    {
        return descriptorSetsLayout;
    }

    inline VkPipelineLayout GetPipelineLayout() const
    {
        return device->SupportsBindless()
                   ? device->GetBindlessDescriptorManager()->GetPipelineLayout()
                   : PipelineLayout;
    }

    // 	inline bool HasDescriptors() const
    // 	{
    // 		return DescriptorSetLayout.GetLayouts().Num() > 0;
    // 	}

    inline uint32 GetDescriptorSetLayoutHash() const
    {
        return descriptorSetsLayout.GetHash();
    }

    void PatchSpirvBindings(VulkanShader::SpirvCode &SpirvCode,
                            ShaderFrequency Frequency, const ShaderHeader &CodeHeader) const;

protected:
    DescriptorSetsLayout descriptorSetsLayout;
    VkPipelineLayout PipelineLayout;

    // 	template <bool bIsCompute>
    // 	inline void FinalizeBindings(const FUniformBufferGatherInfo&
    // UBGatherInfo)
    // 	{
    // 		// Setting descriptor is only allowed prior to compiling the
    // layout 		check(DescriptorSetLayout.GetHandles().Num() == 0);

    // 		DescriptorSetLayout.FinalizeBindings<bIsCompute>(UBGatherInfo);
    // 	}

    // 	inline void ProcessBindingsForStage(VkShaderStageFlagBits StageFlags,
    // ShaderStage::EStage DescSet, const FVulkanShaderHeader& CodeHeader,
    // FUniformBufferGatherInfo& OutUBGatherInfo) const
    // 	{
    // 		// Setting descriptor is only allowed prior to compiling the
    // layout 		check(DescriptorSetLayout.GetHandles().Num() == 0);

    // 		DescriptorSetLayout.ProcessBindingsForStage(StageFlags, DescSet,
    // CodeHeader, OutUBGatherInfo);
    // 	}

    void Compile(DescriptorSetLayoutMap &DSetLayoutMap);

    // 	friend class FVulkanComputePipeline;
    // 	friend class FVulkanGfxPipeline;
    friend class PipelineStateCacheManager;
#if VULKAN_RHI_RAYTRACING
    friend class FVulkanRayTracingPipelineState;
#endif
};

class VulkanGfxLayout : public VulkanPipelineLayout
{
public:
    VulkanGfxLayout(Device *InDevice) : VulkanPipelineLayout(InDevice) {}

    virtual bool IsGfxLayout() const final override { return true; }

    inline const VulkanGfxPipelineDescriptorInfo &GetGfxPipelineDescriptorInfo()
        const
    {
        return GfxPipelineDescriptorInfo;
    }

    // 	bool UsesInputAttachment(FVulkanShaderHeader::EAttachmentType
    // AttachmentType) const;

protected:
    VulkanGfxPipelineDescriptorInfo GfxPipelineDescriptorInfo;
    friend class PipelineStateCacheManager;
};

class VulkanComputeLayout : public VulkanPipelineLayout
{
public:
    VulkanComputeLayout(Device *InDevice)
        : VulkanPipelineLayout(InDevice) {}

    virtual bool IsGfxLayout() const final override { return false; }

    inline const VulkanComputePipelineDescriptorInfo &GetComputePipelineDescriptorInfo() const
    {
        return ComputePipelineDescriptorInfo;
    }

protected:
    VulkanComputePipelineDescriptorInfo ComputePipelineDescriptorInfo;
    friend class PipelineStateCacheManager;
};
