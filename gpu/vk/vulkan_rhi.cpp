#include "descriptor_sets.h"
#include "private.h"
#include "device.h"
#include "renderpass.h"
#include "context.h"
#include "pending_state.h"

#include <algorithm>
#include <numeric>

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

// Increments a value and asserts on overflow.
// FSetInfo uses narrow integer types for descriptor counts,
// which may feasibly overflow one day (for example if we add bindless resources).
template <typename T>
static void IncrementChecked(T &Value)
{
    check(Value < std::numeric_limits<T>::max());
    ++Value;
}

void DescriptorSetsLayoutInfo::AddDescriptor(int32 DescriptorSetIndex, const VkDescriptorSetLayoutBinding &Descriptor)
{
    // Increment type usage
    if (layoutTypes.count(Descriptor.descriptorType))
    {
        layoutTypes[Descriptor.descriptorType]++;
    }
    else
    {
        layoutTypes.insert(std::pair(Descriptor.descriptorType, 1));
    }

    if (DescriptorSetIndex >= setLayouts.size())
    {
        setLayouts.resize(DescriptorSetIndex + 1);
    }

    SetLayout &DescSetLayout = setLayouts[DescriptorSetIndex];

    DescSetLayout.LayoutBindings.push_back(VkDescriptorSetLayoutBinding());
    VkDescriptorSetLayoutBinding *Binding = &DescSetLayout.LayoutBindings.back();
    *Binding = Descriptor;

    const DescriptorSetRemappingInfo::SetInfo &SetInfo = RemappingInfo.SetInfos[DescriptorSetIndex];
    check(SetInfo.Types[Descriptor.binding] == Descriptor.descriptorType);
    switch (Descriptor.descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        IncrementChecked(RemappingInfo.SetInfos[DescriptorSetIndex].NumImageInfos);
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        IncrementChecked(RemappingInfo.SetInfos[DescriptorSetIndex].NumBufferInfos);
        break;
#if VULKAN_RHI_RAYTRACING
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        IncrementChecked(RemappingInfo.SetInfos[DescriptorSetIndex].NumAccelerationStructures);
        break;
#endif
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        break;
    default:
        check(0);
        break;
    }
}

void DescriptorSetsLayoutInfo::GenerateHash(const std::vector<SamplerState *> &InImmutableSamplers)
{
    const int32 LayoutCount = setLayouts.size();
    hash = MemCrc32(&typesUsageID, sizeof(uint32), LayoutCount);

    for (int32 layoutIndex = 0; layoutIndex < LayoutCount; ++layoutIndex)
    {
        setLayouts[layoutIndex].GenerateHash();
        hash = MemCrc32(&setLayouts[layoutIndex].Hash, sizeof(uint32), hash);
    }

    for (uint32 RemapingIndex = 0; RemapingIndex < ShaderStage::NumStages; ++RemapingIndex)
    {
        hash = ::MemCrc32(&RemappingInfo.StageInfos[RemapingIndex].PackedUBDescriptorSet, sizeof(uint16), hash);
        hash = ::MemCrc32(&RemappingInfo.StageInfos[RemapingIndex].Pad0, sizeof(uint16), hash);

        std::vector<DescriptorSetRemappingInfo::RemappingInfo> &Globals = RemappingInfo.StageInfos[RemapingIndex].Globals;
        hash = ::MemCrc32(Globals.data(), sizeof(DescriptorSetRemappingInfo::RemappingInfo) * Globals.size(), hash);

        std::vector<DescriptorSetRemappingInfo::UBRemappingInfo> &UniformBuffers = RemappingInfo.StageInfos[RemapingIndex].UniformBuffers;
        hash = ::MemCrc32(UniformBuffers.data(), sizeof(DescriptorSetRemappingInfo::UBRemappingInfo) * UniformBuffers.size(), hash);

        std::vector<uint16> &PackedUBBindingIndices = RemappingInfo.StageInfos[RemapingIndex].PackedUBBindingIndices;
        hash = ::MemCrc32(PackedUBBindingIndices.data(), sizeof(uint16) * PackedUBBindingIndices.size(), hash);
    }
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