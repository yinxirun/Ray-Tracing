#pragma once

#include <vector>
#include "gpu/core/serialization/archive.h"
#include "gpu/core/containers/enum_as_byte.h"

/** The base type of a value in a shader parameter structure. */
enum EUniformBufferBaseType : uint8
{
    UBMT_INVALID,

    // Invalid type when trying to use bool, to have explicit error message to programmer on why
    // they shouldn't use bool in shader parameter structures.
    UBMT_BOOL,

    // Parameter types.
    UBMT_INT32,
    UBMT_UINT32,
    UBMT_FLOAT32,

    // RHI resources not tracked by render graph.
    UBMT_TEXTURE,
    UBMT_SRV,
    UBMT_UAV,
    UBMT_SAMPLER,

    // Resources tracked by render graph.
    UBMT_RDG_TEXTURE,
    UBMT_RDG_TEXTURE_ACCESS,
    UBMT_RDG_TEXTURE_ACCESS_ARRAY,
    UBMT_RDG_TEXTURE_SRV,
    UBMT_RDG_TEXTURE_UAV,
    UBMT_RDG_BUFFER_ACCESS,
    UBMT_RDG_BUFFER_ACCESS_ARRAY,
    UBMT_RDG_BUFFER_SRV,
    UBMT_RDG_BUFFER_UAV,
    UBMT_RDG_UNIFORM_BUFFER,

    // Nested structure.
    UBMT_NESTED_STRUCT,

    // Structure that is nested on C++ side, but included on shader side.
    UBMT_INCLUDED_STRUCT,

    // GPU Indirection reference of struct, like is currently named Uniform buffer.
    UBMT_REFERENCED_STRUCT,

    // Structure dedicated to setup render targets for a rasterizer pass.
    UBMT_RENDER_TARGET_BINDING_SLOTS,

    EUniformBufferBaseType_Num,
    EUniformBufferBaseType_NumBits = 5,
};
static_assert(EUniformBufferBaseType_Num <= (1 << EUniformBufferBaseType_NumBits), "EUniformBufferBaseType_Num will not fit on EUniformBufferBaseType_NumBits");

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
