#include "descriptor_sets.h"
#include "private.h"
#include "device.h"
#include "renderpass.h"
#include "context.h"
#include "pending_state.h"

#include <algorithm>

// Whether to use VK_ACCESS_SHADER_READ_BIT an input attachments to workaround rendering issues
// 0 use: VK_ACCESS_INPUT_ATTACHMENT_READ_BIT (default)
// 1 use: VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT
int32 GVulkanInputAttachmentShaderRead = 0;

std::atomic<uint64> GVulkanDSetLayoutHandleIdCounter{0};

// 461
void CommandListContext::ReleasePendingState()
{
    delete pendingGfxState;
    pendingGfxState = nullptr;
}

// 1640
DescriptorSetsLayout::DescriptorSetsLayout(Device *InDevice) : device(InDevice) {}

DescriptorSetsLayout::~DescriptorSetsLayout()
{
    // Handles are owned by FVulkanPipelineStateCacheManager
    layoutHandles.resize(0);
}

void DescriptorSetsLayoutInfo::CompileTypesUsageID()
{
    /* FScopeLock ScopeLock(&GTypesUsageCS); */

    static std::unordered_map<uint32, uint32> GTypesUsageHashMap;
    static uint32 GUniqueID = 1;

    std::vector<std::pair<VkDescriptorType, uint32>> temp;
    for (const auto &Elem : layoutTypes)
        temp.push_back(Elem);

    sort(temp.begin(), temp.end(),
         [](std::pair<VkDescriptorType, uint32> &a, std::pair<VkDescriptorType, uint32> &b) -> bool
         {
             return static_cast<uint32>(a.first) < static_cast<uint32>(b.first);
         });

    uint32 TypesUsageHash = 0;
    for (const auto &Elem : temp)
    {
        TypesUsageHash = MemCrc32(&Elem.second, sizeof(uint32), TypesUsageHash);
    }

    // uint32 *UniqueID = GTypesUsageHashMap.Find(TypesUsageHash);
    auto it = GTypesUsageHashMap.find(TypesUsageHash);
    if (it == GTypesUsageHashMap.end())
    {
        GTypesUsageHashMap.insert(std::pair(TypesUsageHash, GUniqueID++));
        typesUsageID = GUniqueID;
    }
    else
    {
        typesUsageID = it->second;
    }
}

void DescriptorSetsLayout::Compile(DescriptorSetLayoutMap &DSetLayoutMap)
{
    check(layoutHandles.size() == 0);

    // Check if we obey limits
    const VkPhysicalDeviceLimits &Limits = device->GetLimits();

    // Check for maxDescriptorSetSamplers
    check(layoutTypes[VK_DESCRIPTOR_TYPE_SAMPLER] + layoutTypes[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] <= Limits.maxDescriptorSetSamplers);

    // Check for maxDescriptorSetUniformBuffers
    check(layoutTypes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER] + layoutTypes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC] <= Limits.maxDescriptorSetUniformBuffers);

    // Check for maxDescriptorSetUniformBuffersDynamic
    check(layoutTypes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC] <= Limits.maxDescriptorSetUniformBuffersDynamic);

    // Check for maxDescriptorSetStorageBuffers
    check(layoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER] + layoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC] <= Limits.maxDescriptorSetStorageBuffers);

    // Check for maxDescriptorSetStorageBuffersDynamic
    check(layoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC] <= Limits.maxDescriptorSetStorageBuffersDynamic);

    // Check for maxDescriptorSetSampledImages
    check(layoutTypes[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] + layoutTypes[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE] + layoutTypes[VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER] <= Limits.maxDescriptorSetSampledImages);

    // Check for maxDescriptorSetStorageImages
    check(layoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_IMAGE] + layoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER] <= Limits.maxDescriptorSetStorageImages);

    check(layoutTypes[VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT] <= Limits.maxDescriptorSetInputAttachments);

#if VULKAN_RHI_RAYTRACING
    if (GRHISupportsRayTracing)
    {
        check(LayoutTypes[VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR] < Device->GetOptionalExtensionProperties().AccelerationStructureProps.maxDescriptorSetAccelerationStructures);
    }
#endif

    layoutHandles.clear();

    if (UseVulkanDescriptorCache())
    {
        layoutHandleIds.clear();
    }

    for (SetLayout &Layout : setLayouts)
    {
        layoutHandles.push_back(VkDescriptorSetLayout{});
        VkDescriptorSetLayout *LayoutHandle = &layoutHandles.back();

        uint32 *LayoutHandleId = nullptr;
        if (UseVulkanDescriptorCache())
        {
            layoutHandleIds.push_back(0);
            LayoutHandleId = &layoutHandleIds.back();
        }

        if (DSetLayoutMap.count(Layout) > 0)
        {
            DescriptorSetLayoutEntry *Found = &DSetLayoutMap[Layout];
            *LayoutHandle = Found->Handle;
            if (LayoutHandleId)
            {
                *LayoutHandleId = Found->HandleId;
            }
            continue;
        }

        VkDescriptorSetLayoutCreateInfo DescriptorLayoutInfo;
        ZeroVulkanStruct(DescriptorLayoutInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        DescriptorLayoutInfo.bindingCount = Layout.LayoutBindings.size();
        DescriptorLayoutInfo.pBindings = Layout.LayoutBindings.data();

        VERIFYVULKANRESULT(vkCreateDescriptorSetLayout(device->GetInstanceHandle(), &DescriptorLayoutInfo, VULKAN_CPU_ALLOCATOR, LayoutHandle));

        if (LayoutHandleId)
        {
            *LayoutHandleId = ++GVulkanDSetLayoutHandleIdCounter;
        }

        DescriptorSetLayoutEntry DescriptorSetLayoutEntry;
        DescriptorSetLayoutEntry.Handle = *LayoutHandle;
        DescriptorSetLayoutEntry.HandleId = LayoutHandleId ? *LayoutHandleId : 0;

        DSetLayoutMap.insert(std::pair(Layout, DescriptorSetLayoutEntry));
    }

    if (typesUsageID == ~0)
    {
        CompileTypesUsageID();
    }

    ZeroVulkanStruct(DescriptorSetAllocateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
    DescriptorSetAllocateInfo.descriptorSetCount = layoutHandles.size();
    DescriptorSetAllocateInfo.pSetLayouts = layoutHandles.data();
}

// 1885
RenderPass::RenderPass(Device &InDevice, const RenderTargetLayout &InRTLayout)
    : Layout(InRTLayout),
      renderPass(VK_NULL_HANDLE),
      NumUsedClearValues(InRTLayout.GetNumUsedClearValues()),
      device(InDevice)
{
    renderPass = CreateVulkanRenderPass(InDevice, InRTLayout);
}

RenderPass::~RenderPass()
{
    device.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::DeferredDeletionQueue2::EType::RenderPass, renderPass);
    renderPass = VK_NULL_HANDLE;
}