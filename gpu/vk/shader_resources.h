#pragma once

#include <vector>
#include "gpu/core/serialization/archive.h"
#include "gpu/core/containers/enum_as_byte.h"
#include "gpu/RHI/RHIDefinitions.h"

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

    // Mostly relevant for Vertex Shaders
    uint32 InOutMask;

    ShaderHeader() = default;
    enum EInit
    {
        EZero
    };
    ShaderHeader(EInit) : InOutMask(0) {}
};

inline Archive &operator<<(Archive &Ar, ShaderHeader &Header)
{
    // Ar << Header.UniformBuffers;
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
