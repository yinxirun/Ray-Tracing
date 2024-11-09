#include <vector>
#include <memory>
#include "gpu/core/misc/crc.h"
#include "descriptor_sets.h"
#include "resources.h"
#include "device.h"
#include "rhi.h"
#include "pipeline.h"
#include "util.h"
#include "configuration.h"

// 0 to not change layouts (eg Set 0 = Vertex, 1 = Pixel, etc)
// 1 to use a new set for common Uniform Buffers
// 2 to collapse all sets into Set 0
static int32 GDescriptorSetLayoutMode = 0;

template <typename ShaderType>
ShaderType *VulkanShaderFactory::CreateShader(std::vector<uint8> &Code, Device *Device)
{
    const uint32 ShaderCodeLen = Code.size();
    const uint32 ShaderCodeCRC = MemCrc32(Code.data(), Code.size());
    const uint64 ShaderKey = ((uint64)ShaderCodeLen | ((uint64)ShaderCodeCRC << 32));

    ShaderType *RetShader = new ShaderType(Device);
    RetShader->Setup(std::move(Code), ShaderKey);
    return RetShader;
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
    if (Container.IsCompressed())
    {
        assert(0);
    }
    else
    {
        std::vector<uint32> code;
        code.resize(Container.SpirvCode.size() / sizeof(uint32));
        memcpy(code.data(), Container.SpirvCode.data(), Container.SpirvCode.size());
        return SpirvCode(std::move(code));
    }
}

void VulkanShader::Setup(std::vector<uint8> &&spirv, uint64 shaderKey)
{
    this->spirvContainer.SpirvCode = spirv;
    this->ShaderKey = std::move(shaderKey);
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

std::shared_ptr<ShaderModule> VulkanShader::CreateHandle(const GfxPipelineDesc &Desc, const VulkanLayout *Layout, uint32 LayoutHash)
{
    // FScopeLock Lock(&VulkanShaderModulesMapCS);
    SpirvCode Spirv = GetPatchedSpirvCode(Desc, Layout);
    std::shared_ptr<ShaderModule> Module = CreateShaderModule(device, Spirv);
    return Module;
}

VulkanShader::SpirvCode VulkanShader::GetPatchedSpirvCode(const GfxPipelineDesc &Desc, const VulkanLayout *Layout)
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

ShaderModule::~ShaderModule()
{
    device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::DeferredDeletionQueue2::EType::ShaderModule, actualShaderModule);
}
Device *ShaderModule::device = nullptr;

// 670
VulkanLayout::VulkanLayout(Device *InDevice)
    : VulkanRHI::DeviceChild(InDevice), descriptorSetsLayout(InDevice), PipelineLayout(VK_NULL_HANDLE)
{
}

VulkanLayout::~VulkanLayout()
{
    if (PipelineLayout != VK_NULL_HANDLE)
    {
        device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::DeferredDeletionQueue2::EType::PipelineLayout, PipelineLayout);
        PipelineLayout = VK_NULL_HANDLE;
    }
}

void VulkanLayout::Compile(DescriptorSetLayoutMap &DSetLayoutMap)
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

// 796
void DescriptorSetsLayoutInfo::ProcessBindingsForStage(VkShaderStageFlagBits StageFlags, ShaderStage::EStage DescSetStage,
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
    printf("Have not implement DescriptorSetsLayoutInfo::FinalizeBindings %s\n", __FILE__);
}

template void DescriptorSetsLayoutInfo::FinalizeBindings<true>(const Device &Device, const UniformBufferGatherInfo &UBGatherInfo, const std::vector<SamplerState *> &ImmutableSamplers);
template void DescriptorSetsLayoutInfo::FinalizeBindings<false>(const Device &Device, const UniformBufferGatherInfo &UBGatherInfo, const std::vector<SamplerState *> &ImmutableSamplers);