#include "descriptor_sets.h"
#include "configuration.h"
#include "device.h"
#include "util.h"
#include <vector>
#include <cstring>

int memcmp(const void *a, const void *b, int length)
{
    const char *x = reinterpret_cast<const char *>(a);
    const char *y = reinterpret_cast<const char *>(b);
    for (int i = 0; i < length; ++i)
    {
        if (*x < *y)
            return -1;
        else if (*x > *y)
            return 1;

        x++;
        y++;
    }
    return 0;
}

DescriptorPool::DescriptorPool(Device *InDevice, const DescriptorSetsLayout &InLayout, uint32_t MaxSetsAllocations)
    : device(InDevice), MaxDescriptorSets(0), NumAllocatedDescriptorSets(0), PeakAllocatedDescriptorSets(0),
      layout(InLayout), descriptorPool(VK_NULL_HANDLE)
{
    // Descriptor sets number required to allocate the max number of descriptor sets layout.
    // When we're hashing pools with types usage ID the descriptor pool can be used for different layouts so the initial layout does not make much sense.
    // In the latter case we'll be probably overallocating the descriptor types but given the relatively small number of max allocations this should not have
    // a serious impact.

    MaxDescriptorSets = MaxSetsAllocations;
    std::vector<VkDescriptorPoolSize> types;
    for (uint32_t typeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; typeIndex <= VK_DESCRIPTOR_TYPE_END_RANGE; ++typeIndex)
    {
        VkDescriptorType descriptorType = (VkDescriptorType)typeIndex;
        uint32_t NumTypesUsed = layout.GetTypesUsed(descriptorType);
        if (NumTypesUsed > 0)
        {
            types.push_back(VkDescriptorPoolSize{});
            VkDescriptorPoolSize &Type = types.back();
            memset(&Type, 0, sizeof(VkDescriptorPoolSize));
            Type.type = descriptorType;
            Type.descriptorCount = NumTypesUsed * MaxSetsAllocations;
        }
    }
}

DescriptorPool::~DescriptorPool()
{
    // DEC_DWORD_STAT_BY(STAT_VulkanNumDescSetsTotal, MaxDescriptorSets);
    // DEC_DWORD_STAT(STAT_VulkanNumDescPools);

    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device->GetInstanceHandle(), descriptorPool, VULKAN_CPU_ALLOCATOR);
        descriptorPool = VK_NULL_HANDLE;
    }
}

void DescriptorPool::Reset()
{
    if (descriptorPool != VK_NULL_HANDLE)
    {
        VERIFYVULKANRESULT(vkResetDescriptorPool(device->GetInstanceHandle(), descriptorPool, 0));
    }
    NumAllocatedDescriptorSets = 0;
}

DescriptorPool *TypedDescriptorPoolSet::PushNewPool()
{
    // Max number of descriptor sets layout allocations
    const uint32_t MaxSetsAllocationsBase = 32;
    // Allow max 128 setS per pool (32 << 2)
    const uint32_t MaxSetsAllocations = MaxSetsAllocationsBase << std::min(poolsCount, 2u);

    auto *NewPool = new DescriptorPool(device, layout, MaxSetsAllocations);

    if (poolListCurrent)
    {
        poolListCurrent->Next = new PoolList(NewPool);
        poolListCurrent = poolListCurrent->Next;
    }
    else
    {
        poolListCurrent = poolListHead = new PoolList(NewPool);
    }
    ++poolsCount;

    return NewPool;
}

TypedDescriptorPoolSet::~TypedDescriptorPoolSet()
{
    for (PoolList *Pool = poolListHead; Pool;)
    {
        PoolList *Next = Pool->Next;

        delete Pool->Element;
        delete Pool;

        Pool = Next;
    }
    poolsCount = 0;
}

void TypedDescriptorPoolSet::Reset()
{
    for (PoolList *Pool = poolListHead; Pool; Pool = Pool->Next)
    {
        Pool->Element->Reset();
    }
    poolListCurrent = poolListHead;
}

void DescriptorPoolSetContainer::Reset()
{
    for (auto &pair : TypedDescriptorPools)
    {
        TypedDescriptorPoolSet *typedPool = pair.second;
        typedPool->Reset();
    }
}

DescriptorPoolsManager::~DescriptorPoolsManager()
{
	for (auto* PoolSet : PoolSets)
	{
		delete PoolSet;
	}
	PoolSets.clear();
}

DescriptorPoolSetContainer &DescriptorPoolsManager::AcquirePoolSetContainer()
{
    //FScopeLock ScopeLock(&CS);

    for (auto *PoolSet : PoolSets)
    {
        if (PoolSet->IsUnused())
        {
            PoolSet->SetUsed(true);
            return *PoolSet;
        }
    }

    DescriptorPoolSetContainer *PoolSet = new DescriptorPoolSetContainer(device);
    PoolSets.push_back(PoolSet);

    return *PoolSet;
}

void DescriptorPoolsManager::ReleasePoolSet(DescriptorPoolSetContainer &PoolSet)
{
    PoolSet.Reset();
    PoolSet.SetUsed(false);
}

BindlessDescriptorManager::BindlessDescriptorManager(Device *InDevice) : VulkanRHI::DeviceChild(InDevice), bIsSupported(false) {}

RHIDescriptorHandle BindlessDescriptorManager::ReserveDescriptor(VkDescriptorType DescriptorType)
{
    if (bIsSupported)
    {
        printf("Fuck!! %s: %s\n", __FILE__, __LINE__);
        // const uint8 SetIndex = GetIndexForDescriptorType(DescriptorType);
        // BindlessSetState& State = BindlessSetStates[SetIndex];
        // const uint32 ResourceIndex = GetFreeResourceIndex(State);
        // return FRHIDescriptorHandle(SetIndex, ResourceIndex);
    }
    return RHIDescriptorHandle();
}

void BindlessDescriptorManager::UpdateImage(RHIDescriptorHandle DescriptorHandle, VkImageView VulkanImage, bool bIsDepthStencil, bool bImmediateUpdate)
{
    if (bIsSupported)
    {
        printf("Fuck!! %s: %s\n", __FILE__, __LINE__);
        // const VkDescriptorType DescriptorType = GetDescriptorTypeForSetIndex(DescriptorHandle.GetRawType());
        // check((DescriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) || (DescriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));

        // VkDescriptorImageInfo DescriptorImageInfo;
        // DescriptorImageInfo.sampler = VK_NULL_HANDLE;
        // DescriptorImageInfo.imageView = ImageView;
        // DescriptorImageInfo.imageLayout = (DescriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ? VK_IMAGE_LAYOUT_GENERAL :
        // 	(bIsDepthStencil ? VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // VkDescriptorDataEXT DescriptorData;
        // DescriptorData.pSampledImage = &DescriptorImageInfo;  // same pointer for storage, it's a union
        // UpdateDescriptor(DescriptorHandle, DescriptorData, bImmediateUpdate);
    }
}
