#pragma once

#include <vector>
#include "gpu/core/serialization/archive.h"
#include "gpu/core/serialization/container_archive.h"
#include "gpu/core/containers/enum_as_byte.h"
#include "gpu/RHI/RHIDefinitions.h"
#include "gpu/render_core/cross_compiler_common.h"
#include <Volk/volk.h>
#include "common.h"

static inline VkDescriptorType BindingToDescriptorType(VulkanBindingType::Type Type)
{
    // Make sure these do NOT alias EPackedTypeName*
    switch (Type)
    {
    case VulkanBindingType::PackedUniformBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case VulkanBindingType::UniformBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case VulkanBindingType::CombinedImageSampler:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case VulkanBindingType::Sampler:
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    case VulkanBindingType::Image:
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case VulkanBindingType::UniformTexelBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    case VulkanBindingType::StorageImage:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case VulkanBindingType::StorageTexelBuffer:
        return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    case VulkanBindingType::StorageBuffer:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case VulkanBindingType::InputAttachment:
        return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
#if RHI_RAYTRACING
    case EVulkanBindingType::AccelerationStructure:
        return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
#endif
    default:
        check(0);
        break;
    }

    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

// Vulkan ParameterMap:
// Buffer Index = EBufferIndex
// Base Offset = Index into the subtype
// Size = Ignored for non-globals
struct ShaderHeader
{
    enum Type
    {
        PackedGlobal,
        Global,
        UniformBuffer,
        Count,
    };

    struct UBResourceInfo
    {
        uint16 SourceUBResourceIndex;
        uint16 OriginalBindingIndex;
        // Index into the Global Array
        uint16 GlobalIndex;
        TEnumAsByte<UniformBufferBaseType> UBBaseType;
        uint8 Pad0 = 0;
    };

    struct UniformBufferInfo
    {
        uint32 LayoutHash = 0;
        uint16 ConstantDataOriginalBindingIndex;
        uint8 bOnlyHasResources;
        uint8 Pad0 = 0;
        std::vector<UBResourceInfo> ResourceEntries;
    };
    std::vector<UniformBufferInfo> UniformBuffers;

    struct GlobalInfo
    {
        uint16 OriginalBindingIndex;
        // If this is UINT16_MAX, it's a regular parameter, otherwise this is the SamplerState portion for a CombinedImageSampler
        // and this is the index into Global for the Texture portion
        uint16 CombinedSamplerStateAliasIndex;
        uint16 TypeIndex;
        // 1 if this is an immutable sampler
        uint8 bImmutableSampler = 0;
        uint8 Pad0 = 0;
    };
    std::vector<GlobalInfo> Globals;
    std::vector<TEnumAsByte<VulkanBindingType::Type>> GlobalDescriptorTypes;

    struct PackedGlobalInfo
    {
        uint16 ConstantDataSizeInFloats;
        CrossCompiler::PackedTypeIndex PackedTypeIndex;
        uint8 PackedUBIndex;
    };
    std::vector<PackedGlobalInfo> PackedGlobals;

    struct PackedUBInfo
    {
        uint32 SizeInBytes;
        uint16 OriginalBindingIndex;
        CrossCompiler::PackedTypeIndex PackedTypeIndex;
        uint8 Pad0 = 0;
        uint32 SPIRVDescriptorSetOffset;
        uint32 SPIRVBindingIndexOffset;
    };
    std::vector<PackedUBInfo> PackedUBs;

    enum class AttachmentType : uint8
    {
        Color0,
        Color1,
        Color2,
        Color3,
        Color4,
        Color5,
        Color6,
        Color7,
        Depth,
        Count,
    };
    struct InputAttachment
    {
        uint16 GlobalIndex;
        AttachmentType Type;
        uint8 Pad = 0;
    };
    std::vector<InputAttachment> InputAttachments;

    // Mostly relevant for Vertex Shaders
    uint32 InOutMask;

    ShaderHeader() = default;
    enum EInit
    {
        EZero
    };
    ShaderHeader(EInit) : InOutMask(0) {}
};

inline Archive &operator<<(Archive &Ar, ShaderHeader::UBResourceInfo &Entry)
{
    Ar << Entry.SourceUBResourceIndex;
    Ar << Entry.OriginalBindingIndex;
    Ar << Entry.GlobalIndex;
    Ar << Entry.UBBaseType;
    return Ar;
}

inline Archive &operator<<(Archive &Ar, ShaderHeader::UniformBufferInfo &UBInfo)
{
    Ar << UBInfo.LayoutHash;
    Ar << UBInfo.ConstantDataOriginalBindingIndex;
    Ar << UBInfo.bOnlyHasResources;
    // Ar << UBInfo.ConstantDataSizeInBytes;
    Ar << UBInfo.ResourceEntries;
    return Ar;
}

inline Archive &operator<<(Archive &Ar, ShaderHeader &Header)
{
    Ar << Header.UniformBuffers;
    // Ar << Header.Globals;
    // Ar << Header.GlobalDescriptorTypes;
    // Ar << Header.PackedGlobals;
    // Ar << Header.PackedUBs;
    // Ar << Header.InputAttachments;
    // Ar << Header.EmulatedUBCopyRanges;
    // Ar << Header.EmulatedUBsCopyInfo;
    Ar << Header.InOutMask;
    // Ar << Header.RayTracingPayloadType;
    // Ar << Header.RayTracingPayloadSize;
    // Ar << Header.SourceHash;
    // Ar << Header.SpirvCRC;
    // Ar << Header.WaveSize;
    // Ar << Header.UniformBufferSpirvInfos;
    // Ar << Header.GlobalSpirvInfos;
    // Ar << Header.DebugName;
    return Ar;
}
