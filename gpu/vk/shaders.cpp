#include <vector>
#include <memory>
#include "gpu/core/serialization/memory_reader.h"
#include "gpu/core/misc/crc.h"
#include "gpu/RHI/RHI.h"

#include "descriptor_sets.h"
#include "resources.h"
#include "device.h"
#include "rhi.h"
#include "pipeline.h"
#include "util.h"
#include "configuration.h"
#include "state.h"

// 0 to not change layouts (eg Set 0 = Vertex, 1 = Pixel, etc)
// 1 to use a new set for common Uniform Buffers
// 2 to collapse all sets into Set 0
static int32 GDescriptorSetLayoutMode = 0;

// default 2
// 2 to treat ALL uniform buffers as dynamic [default]
// 1 to treat global/packed uniform buffers as dynamic
// 0 to treat them as regular
int32 GDynamicGlobalUBs = 0;

VulkanShaderFactory::~VulkanShaderFactory()
{
    for (auto &Map : ShaderMap)
    {
        Map.clear();
    }
}

template <typename ShaderType>
ShaderType *VulkanShaderFactory::CreateShader(std::vector<uint8> &Code, Device *Device)
{
    const uint32 ShaderCodeLen = Code.size();
    const uint32 ShaderCodeCRC = MemCrc32(Code.data(), Code.size());
    const uint64 ShaderKey = ((uint64)ShaderCodeLen | ((uint64)ShaderCodeCRC << 32));

    ShaderType *RetShader = LookupShader<ShaderType>(ShaderKey);

    if (RetShader == nullptr)
    {
        // 反序列化
        MemoryReaderView ar(Code, true);
        ShaderHeader codeHeader;
        ar << codeHeader;
        VulkanShader::SpirvContainer spirvContainer;
        ar << spirvContainer;
        {
            // FRWScopeLock ScopedLock(RWLock[ShaderType::StaticFrequency], SLT_Write);
            auto it = ShaderMap[ShaderType::StaticFrequency].find(ShaderKey);
            if (it != ShaderMap[ShaderType::StaticFrequency].end())
            {
                RetShader = static_cast<ShaderType *>(it->second);
            }
            else
            {
                RetShader = new ShaderType(Device);
                RetShader->Setup(std::move(codeHeader), std::move(spirvContainer), ShaderKey);
                ShaderMap[ShaderType::StaticFrequency].insert(std::pair(ShaderKey, RetShader));
            }
        }
    }
    return RetShader;
}

void VulkanShaderFactory::LookupShaders(const uint64 InShaderKeys[ShaderStage::NumStages], VulkanShader *OutShaders[ShaderStage::NumStages]) const
{
    for (int32 Idx = 0; Idx < ShaderStage::NumStages; ++Idx)
    {
        uint64 ShaderKey = InShaderKeys[Idx];
        if (ShaderKey)
        {
            ShaderFrequency ShaderFrequency = ShaderStage::GetFrequencyForGfxStage((ShaderStage::Stage)Idx);
            /* FRWScopeLock ScopedLock(RWLock[ShaderFrequency], SLT_ReadOnly); */

            auto it = ShaderMap[ShaderFrequency].find(ShaderKey);
            if (it != ShaderMap[ShaderFrequency].end())
            {
                OutShaders[Idx] = it->second;
            }
        }
    }
}

void VulkanShaderFactory::OnDeleteShader(const VulkanShader &Shader)
{
    const uint64 ShaderKey = Shader.GetShaderKey();
    // FRWScopeLock ScopedLock(RWLock[Shader.Frequency], SLT_Write);
    ShaderMap[Shader.Frequency].erase(ShaderKey);
}

VertexShader *RHI::CreateVertexShader(std::vector<uint8> Code)
{
    VertexShader *shader = device->GetShaderFactory().CreateShader<VulkanVertexShader>(Code, device);
    return shader;
}

PixelShader *RHI::CreatePixelShader(std::vector<uint8> Code)
{
    PixelShader *shader = device->GetShaderFactory().CreateShader<VulkanPixelShader>(Code, device);
    return shader;
}

VulkanShader::~VulkanShader() {}

VulkanShader::SpirvCode VulkanShader::GetSpirvCode(const SpirvContainer &Container)
{
    std::vector<uint32> code;
    code.resize(Container.SpirvCode.size() / sizeof(uint32));
    memcpy(code.data(), Container.SpirvCode.data(), Container.SpirvCode.size());
    return SpirvCode(std::move(code));
}

void VulkanShader::Setup(ShaderHeader &&header, SpirvContainer &&spirv, uint64 shaderKey)
{
    check(device);
    this->CodeHeader = std::move(header);
    this->spirvContainer = std::move(spirv);
    this->shaderKey = shaderKey;
}

static std::shared_ptr<ShaderModule> CreateShaderModule(Device *device, VulkanShader::SpirvCode &SpirvCode)
{
    const std::vector<uint32> Spirv = SpirvCode.GetCodeView();
    VkShaderModule ShaderModuleHandle;
    VkShaderModuleCreateInfo ModuleCreateInfo;
    ZeroVulkanStruct(ModuleCreateInfo, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);

    ModuleCreateInfo.codeSize = Spirv.size() * sizeof(uint32);
    ModuleCreateInfo.pCode = Spirv.data();

    VERIFYVULKANRESULT(vkCreateShaderModule(device->GetInstanceHandle(), &ModuleCreateInfo, VULKAN_CPU_ALLOCATOR, &ShaderModuleHandle));

    std::shared_ptr<ShaderModule> ReturnPtr = std::make_shared<ShaderModule>(device, ShaderModuleHandle);

    return ReturnPtr;
}

std::shared_ptr<ShaderModule> VulkanShader::CreateHandle(const GfxPipelineDesc &Desc, const VulkanPipelineLayout *Layout, uint32 LayoutHash)
{
    // FScopeLock Lock(&VulkanShaderModulesMapCS);
    SpirvCode Spirv = GetPatchedSpirvCode(Desc, Layout);
    std::shared_ptr<ShaderModule> Module = CreateShaderModule(device, Spirv);
    return Module;
}

VulkanShader::SpirvCode VulkanShader::GetPatchedSpirvCode(const GfxPipelineDesc &Desc, const VulkanPipelineLayout *Layout)
{
    SpirvCode Spirv = GetSpirvCode(spirvContainer);

    // Layout->PatchSpirvBindings(Spirv, Frequency, CodeHeader);
    if (NeedsSpirvInputAttachmentPatching(Desc))
    {
        // Spirv = PatchSpirvInputAttachments(Spirv);
    }

    return Spirv;
}

bool VulkanShader::NeedsSpirvInputAttachmentPatching(const GfxPipelineDesc &Desc) const
{
    // return (Desc.RasterizationSamples > 1 && CodeHeader.InputAttachments.Num() > 0);
    return false;
}

Archive &operator<<(Archive &Ar, VulkanShader::SpirvContainer &spirvContainer)
{
    uint32 SpirvCodeSizeInBytes;
    Ar << SpirvCodeSizeInBytes;
    check(SpirvCodeSizeInBytes);
    check(Ar.IsLoading());

    std::vector<uint8> &SpirvCode = spirvContainer.SpirvCode;

    SpirvCode.resize(SpirvCodeSizeInBytes);
    Ar.Serialize(SpirvCode.data(), SpirvCodeSizeInBytes);

    return Ar;
}

ShaderModule::~ShaderModule()
{
    device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::DeferredDeletionQueue2::EType::ShaderModule, actualShaderModule);
}
Device *ShaderModule::device = nullptr;

// 670
VulkanPipelineLayout::VulkanPipelineLayout(Device *InDevice)
    : VulkanRHI::DeviceChild(InDevice), descriptorSetsLayout(InDevice), PipelineLayout(VK_NULL_HANDLE)
{
}

VulkanPipelineLayout::~VulkanPipelineLayout()
{
    if (PipelineLayout != VK_NULL_HANDLE)
    {
        device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::DeferredDeletionQueue2::EType::PipelineLayout, PipelineLayout);
        PipelineLayout = VK_NULL_HANDLE;
    }
}

void VulkanPipelineLayout::Compile(DescriptorSetLayoutMap &DSetLayoutMap)
{
    check(PipelineLayout == VK_NULL_HANDLE);

    descriptorSetsLayout.Compile(DSetLayoutMap);

    if (!device->SupportsBindless())
    {
        VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo;
        ZeroVulkanStruct(PipelineLayoutCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);

        const std::vector<VkDescriptorSetLayout> &LayoutHandles = descriptorSetsLayout.GetHandles();
        PipelineLayoutCreateInfo.setLayoutCount = LayoutHandles.size();
        PipelineLayoutCreateInfo.pSetLayouts = LayoutHandles.data();
        // PipelineLayoutCreateInfo.pushConstantRangeCount = 0;
        VERIFYVULKANRESULT(vkCreatePipelineLayout(device->GetInstanceHandle(), &PipelineLayoutCreateInfo, VULKAN_CPU_ALLOCATOR, &PipelineLayout));
    }
}

// 719
uint32 DescriptorSetWriter::SetupDescriptorWrites(
    const std::vector<VkDescriptorType> &Types, HashableDescriptorInfo *InHashableDescriptorInfos,
    VkWriteDescriptorSet *InWriteDescriptors, VkDescriptorImageInfo *InImageInfo, VkDescriptorBufferInfo *InBufferInfo,
    uint8 *InBindingToDynamicOffsetMap,
#if VULKAN_RHI_RAYTRACING
    VkWriteDescriptorSetAccelerationStructureKHR *InAccelerationStructuresWriteDescriptors,
    VkAccelerationStructureKHR *InAccelerationStructures,
#endif // VULKAN_RHI_RAYTRACING
    const VulkanSamplerState &DefaultSampler, const View::TextureView &DefaultImageView)
{
    this->HashableDescriptorInfos = InHashableDescriptorInfos;
    this->WriteDescriptors = InWriteDescriptors;
    this->NumWrites = Types.size();

    this->BindingToDynamicOffsetMap = InBindingToDynamicOffsetMap;

    InitWrittenMasks(NumWrites);

    uint32 DynamicOffsetIndex = 0;

    for (int32 Index = 0; Index < Types.size(); ++Index)
    {
        InWriteDescriptors->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        InWriteDescriptors->dstBinding = Index;
        InWriteDescriptors->descriptorCount = 1;
        InWriteDescriptors->descriptorType = Types[Index];

        switch (Types[Index])
        {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            BindingToDynamicOffsetMap[Index] = DynamicOffsetIndex;
            ++DynamicOffsetIndex;
            InWriteDescriptors->pBufferInfo = InBufferInfo++;
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            InWriteDescriptors->pBufferInfo = InBufferInfo++;
            break;
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            SetWrittenBase(Index); // samplers have a default setting, don't assert on those yet.
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            // Texture.Load() still requires a default sampler...
            if (InHashableDescriptorInfos) // UseVulkanDescriptorCache()
            {
                InHashableDescriptorInfos[Index].Image.SamplerId = DefaultSampler.SamplerId;
                InHashableDescriptorInfos[Index].Image.ImageViewId = DefaultImageView.ViewId;
                InHashableDescriptorInfos[Index].Image.ImageLayout = static_cast<uint32>(VK_IMAGE_LAYOUT_GENERAL);
            }
            InImageInfo->sampler = DefaultSampler.sampler;
            InImageInfo->imageView = DefaultImageView.View;
            InImageInfo->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            InWriteDescriptors->pImageInfo = InImageInfo++;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            break;
#if VULKAN_RHI_RAYTRACING
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            InAccelerationStructuresWriteDescriptors->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            InAccelerationStructuresWriteDescriptors->pNext = nullptr;
            InAccelerationStructuresWriteDescriptors->accelerationStructureCount = 1;
            InAccelerationStructuresWriteDescriptors->pAccelerationStructures = InAccelerationStructures++;
            InWriteDescriptors->pNext = InAccelerationStructuresWriteDescriptors++;
            break;
#endif // VULKAN_RHI_RAYTRACING
        default:
            check(0);
            break;
        }
        ++InWriteDescriptors;
    }

    return DynamicOffsetIndex;
}

// 796
void DescriptorSetsLayoutInfo::ProcessBindingsForStage(VkShaderStageFlagBits StageFlags, ShaderStage::Stage DescSetStage,
                                                       const ShaderHeader &CodeHeader, UniformBufferGatherInfo &OutUBGatherInfo) const
{
    const bool bMoveCommonUBsToExtraSet = GDescriptorSetLayoutMode == 1 || GDescriptorSetLayoutMode == 2;

    // Find all common UBs from different stages
    for (const ShaderHeader::UniformBufferInfo &UBInfo : CodeHeader.UniformBuffers)
    {
        if (bMoveCommonUBsToExtraSet)
        {
            printf("ERROR: Don't support MoveCommonUBsToExtraSet %s\n", __FILE__);
            exit(-1);
        }
        else
        {
            OutUBGatherInfo.UBLayoutsToUsedStageMap.insert(std::pair(UBInfo.LayoutHash, (VkShaderStageFlags)StageFlags));
        }
    }

    OutUBGatherInfo.CodeHeaders[DescSetStage] = &CodeHeader;
}

// 838
template <bool bIsCompute>
void DescriptorSetsLayoutInfo::FinalizeBindings(const Device &device, const UniformBufferGatherInfo &UBGatherInfo, const std::vector<SamplerState *> &ImmutableSamplers)
{
    check(!bIsCompute);
    check(RemappingInfo.IsEmpty());

    std::unordered_map<uint32, DescriptorSetRemappingInfo::UBRemappingInfo> AlreadyProcessedUBs;

    // We'll be reusing this struct
    VkDescriptorSetLayoutBinding Binding;
    Binding = {};
    Binding.descriptorCount = 1;

    const bool bConvertAllUBsToDynamic = (GDynamicGlobalUBs > 1);
    const bool bConvertPackedUBsToDynamic = bConvertAllUBsToDynamic || GDynamicGlobalUBs == 1;
    const bool bConsolidateAllIntoOneSet = GDescriptorSetLayoutMode == 2;
    const uint32 MaxDescriptorSetUniformBuffersDynamic = device.GetLimits().maxDescriptorSetUniformBuffersDynamic;

    uint8 DescriptorStageToSetMapping[ShaderStage::NumStages];
    memset(DescriptorStageToSetMapping, UINT8_MAX, sizeof(uint8) * ShaderStage::NumStages);

    const bool bMoveCommonUBsToExtraSet = (UBGatherInfo.CommonUBLayoutsToStageMap.size() > 0) || bConsolidateAllIntoOneSet;

    uint32 CommonUBDescriptorSet;
    if (bMoveCommonUBsToExtraSet)
    {
        CommonUBDescriptorSet = RemappingInfo.SetInfos.size();
        RemappingInfo.SetInfos.push_back(DescriptorSetRemappingInfo::SetInfo());
    }
    else
    {
        CommonUBDescriptorSet = UINT32_MAX;
    }

    auto FindOrAddDescriptorSet = [&](int32 Stage) -> uint8
    {
        if (bConsolidateAllIntoOneSet)
        {
            return 0;
        }

        // 每个阶段一个set
        if (DescriptorStageToSetMapping[Stage] == UINT8_MAX)
        {
            uint32 NewSet = RemappingInfo.SetInfos.size();
            RemappingInfo.SetInfos.push_back(DescriptorSetRemappingInfo::SetInfo());
            DescriptorStageToSetMapping[Stage] = (uint8)NewSet;
            return NewSet;
        }

        return DescriptorStageToSetMapping[Stage];
    };

    int32 CurrentImmutableSampler = 0;
    for (int32 Stage = 0; Stage < (bIsCompute ? 1 : ShaderStage::NumStages); ++Stage)
    {
        if (const ShaderHeader *ShaderHeader = UBGatherInfo.CodeHeaders[Stage])
        {
            VkShaderStageFlags StageFlags = UEFrequencyToVKStageBit(bIsCompute ? SF_Compute : ShaderStage::GetFrequencyForGfxStage((ShaderStage::Stage)Stage));
            Binding.stageFlags = StageFlags;

            RemappingInfo.StageInfos[Stage].PackedUBBindingIndices.reserve(ShaderHeader->PackedUBs.size());
            for (int32 Index = 0; Index < ShaderHeader->PackedUBs.size(); ++Index)
            {
                check(0);
            }

            RemappingInfo.StageInfos[Stage].UniformBuffers.reserve(ShaderHeader->UniformBuffers.size());
            for (int32 Index = 0; Index < ShaderHeader->UniformBuffers.size(); ++Index)
            {
                VkDescriptorType Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                if (bConvertAllUBsToDynamic && layoutTypes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC] < MaxDescriptorSetUniformBuffersDynamic)
                {
                    Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                }

                // Here we might mess up with the stageFlags, so reset them every loop
                Binding.stageFlags = StageFlags;
                Binding.descriptorType = Type;
                const ShaderHeader::UniformBufferInfo &UBInfo = ShaderHeader->UniformBuffers[Index];
                const uint32 LayoutHash = UBInfo.LayoutHash;
                const bool bUBHasConstantData = UBInfo.ConstantDataOriginalBindingIndex != UINT16_MAX;
                if (bUBHasConstantData)
                {
                    bool bProcessRegularUB = true;
                    const VkShaderStageFlags *FoundFlags = bMoveCommonUBsToExtraSet ? &(UBGatherInfo.CommonUBLayoutsToStageMap.find(LayoutHash)->second) : nullptr;
                    if (FoundFlags)
                    {
                        check(0);
                    }

                    if (bProcessRegularUB)
                    {
                        int32 DescriptorSet = FindOrAddDescriptorSet(Stage);
                        uint32 NewBindingIndex;
                        RemappingInfo.AddUBWithData(Stage, Index, DescriptorSet, Type, NewBindingIndex);
                        Binding.binding = NewBindingIndex;

                        AddDescriptor(FindOrAddDescriptorSet(Stage), Binding);
                    }
                }
                else
                {
                    RemappingInfo.AddUBResourceOnly(Stage, Index);
                }
            }

            RemappingInfo.StageInfos[Stage].Globals.reserve(ShaderHeader->Globals.size());
            Binding.stageFlags = StageFlags;
            for (int32 Index = 0; Index < ShaderHeader->Globals.size(); ++Index)
            {
                const ShaderHeader::GlobalInfo &GlobalInfo = ShaderHeader->Globals[Index];
                int32 DescriptorSet = FindOrAddDescriptorSet(Stage);
                VkDescriptorType Type = BindingToDescriptorType(ShaderHeader->GlobalDescriptorTypes[GlobalInfo.TypeIndex]);
                uint16 CombinedSamplerStateAlias = GlobalInfo.CombinedSamplerStateAliasIndex;
                uint32 NewBindingIndex = RemappingInfo.AddGlobal(Stage, Index, DescriptorSet, Type, CombinedSamplerStateAlias);
                Binding.binding = NewBindingIndex;
                Binding.descriptorType = Type;
                if (CombinedSamplerStateAlias == UINT16_MAX)
                {
                    if (GlobalInfo.bImmutableSampler)
                    {
                        if (CurrentImmutableSampler < ImmutableSamplers.size())
                        {
                            VulkanSamplerState *samplerState = static_cast<VulkanSamplerState *>(ImmutableSamplers[CurrentImmutableSampler]);
                            if (samplerState && samplerState->sampler != VK_NULL_HANDLE)
                            {
                                Binding.pImmutableSamplers = &samplerState->sampler;
                            }
                            ++CurrentImmutableSampler;
                        }
                    }

                    AddDescriptor(DescriptorSet, Binding);
                }

                Binding.pImmutableSamplers = nullptr;
            }

            if (ShaderHeader->InputAttachments.size())
            {
                printf("Error: Don't support %s %d\n", __FILE__, __LINE__);
                exit(-1);
                /* int32 DescriptorSet = FindOrAddDescriptorSet(Stage);
                check(Stage == ShaderStage::Pixel);
                for (int32 SrcIndex = 0; SrcIndex < ShaderHeader->InputAttachments.Num(); ++SrcIndex)
                {
                    int32 OriginalGlobalIndex = ShaderHeader->InputAttachments[SrcIndex].GlobalIndex;
                    const FVulkanShaderHeader::FGlobalInfo &OriginalGlobalInfo = ShaderHeader->Globals[OriginalGlobalIndex];
                    check(BindingToDescriptorType(ShaderHeader->GlobalDescriptorTypes[OriginalGlobalInfo.TypeIndex]) == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
                    int32 RemappingIndex = RemappingInfo.InputAttachmentData.AddDefaulted();
                    FInputAttachmentData &AttachmentData = RemappingInfo.InputAttachmentData[RemappingIndex];
                    AttachmentData.BindingIndex = RemappingInfo.StageInfos[Stage].Globals[OriginalGlobalIndex].NewBindingIndex;
                    AttachmentData.DescriptorSet = (uint8)DescriptorSet;
                    AttachmentData.Type = ShaderHeader->InputAttachments[SrcIndex].Type;
                } */
            }
        }
    }

    CompileTypesUsageID();
    GenerateHash(ImmutableSamplers);

    // If we are consolidating and no uniforms are present in the shader, then strip the empty set data
    if (bConsolidateAllIntoOneSet)
    {
        printf("Don't support %s %d\n", __FILE__, __LINE__);
        exit(-1);
    }
    else
    {
        for (int32 Index = 0; Index < RemappingInfo.SetInfos.size(); ++Index)
        {
            check(RemappingInfo.SetInfos[Index].Types.size() > 0);
        }
    }
}

void VulkanGfxPipelineDescriptorInfo::Initialize(const DescriptorSetRemappingInfo &InRemappingInfo)
{
    check(!bInitialized);

    for (int32 StageIndex = 0; StageIndex < ShaderStage::NumStages; ++StageIndex)
    {
        // #todo-rco: Enable this!
        RemappingUBInfos[StageIndex].resize(InRemappingInfo.StageInfos[StageIndex].UniformBuffers.size());
        for (int i = 0; i < RemappingUBInfos[StageIndex].size(); ++i)
        {
            RemappingUBInfos[StageIndex][i] = &InRemappingInfo.StageInfos[StageIndex].UniformBuffers[i];
        }

        RemappingGlobalInfos[StageIndex].resize(InRemappingInfo.StageInfos[StageIndex].Globals.size());
        for (int i = 0; i < RemappingGlobalInfos[StageIndex].size(); ++i)
        {
            RemappingGlobalInfos[StageIndex][i] = &InRemappingInfo.StageInfos[StageIndex].Globals[i];
        }

        RemappingPackedUBInfos[StageIndex].resize(InRemappingInfo.StageInfos[StageIndex].PackedUBBindingIndices.size());
        for (int i = 0; i < RemappingPackedUBInfos[StageIndex].size(); ++i)
        {
            RemappingPackedUBInfos[StageIndex][i] = &InRemappingInfo.StageInfos[StageIndex].PackedUBBindingIndices[i];
        }
    }

    RemappingInfo = &InRemappingInfo;

    for (int32 Index = 0; Index < InRemappingInfo.SetInfos.size(); ++Index)
    {
        const DescriptorSetRemappingInfo::SetInfo &SetInfo = InRemappingInfo.SetInfos[Index];
        if (SetInfo.Types.size() > 0)
        {
            check(Index < sizeof(HasDescriptorsInSetMask) * 8);
            HasDescriptorsInSetMask = HasDescriptorsInSetMask | (1 << Index);
        }
        else
        {
            ensure(0);
        }
    }

    bInitialized = true;
}

template void DescriptorSetsLayoutInfo::FinalizeBindings<true>(const Device &Device, const UniformBufferGatherInfo &UBGatherInfo, const std::vector<SamplerState *> &ImmutableSamplers);
template void DescriptorSetsLayoutInfo::FinalizeBindings<false>(const Device &Device, const UniformBufferGatherInfo &UBGatherInfo, const std::vector<SamplerState *> &ImmutableSamplers);