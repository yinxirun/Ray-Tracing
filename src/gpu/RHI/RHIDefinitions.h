#pragma once
#include "../definitions.h"

#define PRINT_UNIMPLEMENT
#undef PRINT_UNIMPLEMENT

enum class EClearBinding
{
	ENoneBound,			// no clear color associated with this target.  Target will not do hardware clears on most platforms
	EColorBound,		// target has a clear color bound.  Clears will use the bound color, and do hardware clears.
	EDepthStencilBound, // target has a depthstencil value bound.  Clears will use the bound values and do hardware clears.
};

/** Maximum number of miplevels in a texture. */
enum { MAX_TEXTURE_MIP_COUNT = 15 };

struct ClearValueBinding
{
	struct DSValue
	{
		float Depth;
		uint32 Stencil;
	};

	EClearBinding ColorBinding;

	union ClearValueType
	{
		float Color[4];
		DSValue DSValue;
	} Value;
};

enum class ERHIDescriptorHeapType : uint8
{
	Standard,
	Sampler,
	RenderTarget,
	DepthStencil,
	Count,
	Invalid = 0xff
};

/** An enumeration of the different RHI reference types. */
enum ERHIResourceType : uint8
{
	RRT_None,

	RRT_SamplerState,
	RRT_RasterizerState,
	RRT_DepthStencilState,
	RRT_BlendState,
	RRT_VertexDeclaration,
	RRT_VertexShader,
	RRT_MeshShader,
	RRT_AmplificationShader,
	RRT_PixelShader,
	RRT_GeometryShader,
	RRT_RayTracingShader,
	RRT_ComputeShader,
	RRT_GraphicsPipelineState,
	RRT_ComputePipelineState,
	RRT_RayTracingPipelineState,
	RRT_BoundShaderState,
	RRT_UniformBufferLayout,
	RRT_UniformBuffer,
	RRT_Buffer,
	RRT_Texture,
	// @todo: texture type unification - remove these
	RRT_Texture2D,
	RRT_Texture2DArray,
	RRT_Texture3D,
	RRT_TextureCube,
	// @todo: texture type unification - remove these
	RRT_TextureReference,
	RRT_TimestampCalibrationQuery,
	RRT_GPUFence,
	RRT_RenderQuery,
	RRT_RenderQueryPool,
	RRT_Viewport,
	RRT_UnorderedAccessView,
	RRT_ShaderResourceView,
	RRT_RayTracingAccelerationStructure,
	RRT_StagingBuffer,
	RRT_CustomPresent,
	RRT_ShaderLibrary,
	RRT_PipelineBinaryLibrary,
	RRT_ShaderBundle,

	RRT_Num
};

/** Describes the dimension of a texture. */
enum class ETextureDimension : uint8
{
	Texture2D,
	Texture2DArray,
	Texture3D,
	TextureCube,
	TextureCubeArray
};

struct RHIDescriptorHandle
{
	RHIDescriptorHandle() = default;
	RHIDescriptorHandle(ERHIDescriptorHeapType InType, uint32 InIndex)
		: Index(InIndex), Type((uint8)InType)
	{
	}
	RHIDescriptorHandle(uint8 InType, uint32 InIndex)
		: Index(InIndex), Type(InType)
	{
	}

	inline uint32 GetIndex() const { return Index; }
	inline ERHIDescriptorHeapType GetType() const { return (ERHIDescriptorHeapType)Type; }
	inline uint8 GetRawType() const { return Type; }

	inline bool IsValid() const { return Index != 0xffffffff && Type != (uint8)ERHIDescriptorHeapType::Invalid; }

private:
	uint32 Index{0xffffffff};
	uint8 Type{(uint8)ERHIDescriptorHeapType::Invalid};
};