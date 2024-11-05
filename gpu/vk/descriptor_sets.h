#pragma once
#include <map>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "../definitions.h"
#include <list>
#include "Volk/volk.h"
#include "gpu/RHI/RHIDefinitions.h"
#include "vulkan_memory.h"

class Device;

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
            int length = sizeof(VkDescriptorSetLayoutBinding) * LayoutBindings.size();
            const uint8_t *data = reinterpret_cast<const uint8_t *>(LayoutBindings.data());
            uint32_t crc = 0xFFFFFFFF;
            while (length--)
            {
                crc ^= *data++;
                for (int i = 0; i < 8; i++)
                {
                    if (crc & 1)
                    {
                        crc = (crc >> 1) ^ 0xEDB88320; // 0xEDB88320 是 CRC32 多项式
                    }
                    else
                    {
                        crc >>= 1;
                    }
                }
            }
            Hash = crc ^ 0xFFFFFFFF;
            // Hash = FCrc::MemCrc32(LayoutBindings.data(), sizeof(VkDescriptorSetLayoutBinding) * LayoutBindings.size());
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

        for (int32_t Index = 0; Index < In.setLayouts.size(); ++Index)
        {
            if (In.setLayouts[Index] != setLayouts[Index])
            {
                return false;
            }
        }

        // if (RemappingInfo != In.RemappingInfo)
        // {
        //     return false;
        // }

        return true;
    }

    const std::vector<SetLayout> &GetLayouts() const { return setLayouts; }

protected:
    std::unordered_map<VkDescriptorType, uint32_t> layoutTypes;
    std::vector<SetLayout> setLayouts;
    uint32_t hash = 0;
    uint32_t typesUsageID = ~0;
    VkPipelineBindPoint BindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
};

// The actual run-time descriptor set layouts
class DescriptorSetsLayout : public DescriptorSetsLayoutInfo
{
public:
    DescriptorSetsLayout(Device *InDevice);
    ~DescriptorSetsLayout();

private:
    Device *device;
    std::vector<VkDescriptorSetLayout> layoutHandles;
    std::vector<uint32_t> layoutHandleIds;
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
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

    RHIDescriptorHandle ReserveDescriptor(VkDescriptorType DescriptorType);

    void UpdateImage(RHIDescriptorHandle DescriptorHandle, VkImageView VulkanImage, bool bIsDepthStencil, bool bImmediateUpdate = true);

private:
    const bool bIsSupported = false;
};