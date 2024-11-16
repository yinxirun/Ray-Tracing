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
    struct UBResourceInfo
    {
        uint16 SourceUBResourceIndex;
        uint16 OriginalBindingIndex;
        // Index into the Global Array
        uint16 GlobalIndex;
        TEnumAsByte<EUniformBufferBaseType> UBBaseType;
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

    // Mostly relevant for Vertex Shaders
    uint32 InOutMask;

    ShaderHeader() = default;
    enum EInit
    {
        EZero
    };
    ShaderHeader(EInit) : InOutMask(0) {}
};


inline Archive& operator<<(Archive& Ar, ShaderHeader& Header)
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
