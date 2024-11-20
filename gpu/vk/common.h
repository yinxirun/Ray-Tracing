#pragma once

#include <cstdint>
#include <cstring>
#include "gpu/RHI/RHIDefinitions.h"

namespace ShaderStage
{
	enum Stage
	{
		// Adjusting these requires a full shader rebuild (ie modify the guid on VulkanCommon.usf)
		// Keep the values in sync with EShaderFrequency
		Vertex = 0,
		Pixel = 1,

#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		Geometry = 2,
#endif

#if RHI_RAYTRACING
		RayGen = 3,
		RayMiss = 4,
		RayHitGroup = 5,
		RayCallable = 6,
#endif

#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		NumGeometryStages = 1,
#else
		NumGeometryStages = 0,
#endif

#if RHI_RAYTRACING
		NumRayTracingStages = 4,
#else
		NumRayTracingStages = 0,
#endif

		NumStages = (2 + NumGeometryStages + NumRayTracingStages),

		// Compute is its own pipeline, so it can all live as set 0
		Compute = 0,

#if VULKAN_SUPPORTS_GEOMETRY_SHADERS || RHI_RAYTRACING
		MaxNumSets = 8,
#else
		MaxNumSets = 4,
#endif

		Invalid = -1,
	};

	inline Stage GetStageForFrequency(ShaderFrequency Stage)
	{
		switch (Stage)
		{
		case SF_Vertex:
			return Vertex;
		case SF_Pixel:
			return Pixel;
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		case SF_Geometry:
			return Geometry;
#endif
#if RHI_RAYTRACING
		case SF_RayGen:
			return RayGen;
		case SF_RayMiss:
			return RayMiss;
		case SF_RayHitGroup:
			return RayHitGroup;
		case SF_RayCallable:
			return RayCallable;
#endif // RHI_RAYTRACING
		case SF_Compute:
			return Compute;
		default:
			check(0);
			break;
		}

		return Invalid;
	}

	inline ShaderFrequency GetFrequencyForGfxStage(Stage Stage)
	{
		switch (Stage)
		{
		case Stage::Vertex:
			return SF_Vertex;
		case Stage::Pixel:
			return SF_Pixel;
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		case EStage::Geometry:
			return SF_Geometry;
#endif
#if RHI_RAYTRACING
		case EStage::RayGen:
			return SF_RayGen;
		case EStage::RayMiss:
			return SF_RayMiss;
		case EStage::RayHitGroup:
			return SF_RayHitGroup;
		case EStage::RayCallable:
			return SF_RayCallable;
#endif //	RHI_RAYTRACING
		default:
			// checkf(0, TEXT("Invalid shader Stage %d"), (int32)Stage);
			check(0);
			break;
		}

		return SF_NumFrequencies;
	}
};

namespace VulkanBindingType
{
	enum Type : uint8
	{
		PackedUniformBuffer, // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
		UniformBuffer,		 // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER

		CombinedImageSampler, // VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
		Sampler,			  // VK_DESCRIPTOR_TYPE_SAMPLER
		Image,				  // VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE

		UniformTexelBuffer, // VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER	Buffer<>

		// A storage image (VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) is a descriptor type that is used for load, store, and atomic operations on image memory from within shaders bound to pipelines.
		StorageImage, // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE		RWTexture

		// RWBuffer/RWTexture?
		// A storage texel buffer (VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER) represents a tightly packed array of homogeneous formatted data that is stored in a buffer and is made accessible to shaders. Storage texel buffers differ from uniform texel buffers in that they support stores and atomic operations in shaders, may support a different maximum length, and may have different performance characteristics.
		StorageTexelBuffer,

		// UAV/RWBuffer
		// A storage buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) is a region of structured storage that supports both read and write access for shaders.In addition to general read and write operations, some members of storage buffers can be used as the target of atomic operations.In general, atomic operations are only supported on members that have unsigned integer formats.
		StorageBuffer,

		InputAttachment,

		AccelerationStructure, // VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR

		Count,
	};
}

template <typename T>
static void ZeroVulkanStruct(T &Struct, int32_t VkStructureType)
{
	static_assert(!std::is_pointer<T>::value, "Don't use a pointer!");
	static_assert(offsetof(T, sType) == 0, "Assumes sType is the first member in the Vulkan type!");
	static_assert(sizeof(T::sType) == sizeof(int32_t), "Assumed sType is compatible with int32!");
	// Horrible way to coerce the compiler to not have to know what T::sType is so we can have this header not have to include vulkan.h
	(int32_t &)Struct.sType = VkStructureType;
	memset(((uint8_t *)&Struct) + sizeof(VkStructureType), 0, sizeof(T) - sizeof(VkStructureType));
}