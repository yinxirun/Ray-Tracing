#pragma once
#include "../definitions.h"
#include "gpu/math/color.h"

// 用于控制是否打印未实现函数的信息
#define PRINT_UNIMPLEMENT
#undef PRINT_UNIMPLEMENT

#define PRINT_SEPERATOR_RHI_UNIMPLEMENT
#undef PRINT_SEPERATOR_RHI_UNIMPLEMENT

#define PRINT_TO_EXPLORE
#undef PRINT_TO_EXPLORE

#define DEPRECATED(x, y)

/// 不区分工作线程、渲染线程和RHI线程，所以全部返回true
inline bool IsInGameThread() { return true; }
/// 不区分工作线程、渲染线程和RHI线程，所以全部返回true
inline bool IsInRenderingThread() { return true; }
/// 不区分工作线程、渲染线程和RHI线程，所以全部返回true
inline bool IsInRHIThread() { return true; }

// RHICreateUniformBuffer assumes C++ constant layout matches the shader layout when extracting float constants, yet the C++ struct contains pointers.  
// Enforce a min size of 64 bits on pointer types in uniform buffer structs to guarantee layout matching between languages.
#define SHADER_PARAMETER_POINTER_ALIGNMENT sizeof(uint64)
static_assert(sizeof(void*) <= SHADER_PARAMETER_POINTER_ALIGNMENT, "The alignment of pointer needs to match the largest pointer.");


// 180
enum class ERHIZBuffer
{
	// Before changing this, make sure all math & shader assumptions are correct! Also wrap your C++ assumptions with
	//		static_assert(ERHIZBuffer::IsInvertedZBuffer(), ...);
	// Shader-wise, make sure to update Definitions.usf, HAS_INVERTED_Z_BUFFER
	FarPlane = 0,
	NearPlane = 1,

	// 'bool' for knowing if the API is using Inverted Z buffer
	IsInverted = (int32)((int32)ERHIZBuffer::FarPlane < (int32)ERHIZBuffer::NearPlane),
};

// 239
enum ERasterizerFillMode
{
	FM_Point,
	FM_Wireframe,
	FM_Solid,

	ERasterizerFillMode_Num,
	ERasterizerFillMode_NumBits = 2,
};
static_assert(ERasterizerFillMode_Num <= (1 << ERasterizerFillMode_NumBits), "ERasterizerFillMode_Num will not fit on ERasterizerFillMode_NumBits");

enum ERasterizerCullMode
{
	CM_None,
	CM_CW,
	CM_CCW,

	ERasterizerCullMode_Num,
	ERasterizerCullMode_NumBits = 2,
};
static_assert(ERasterizerCullMode_Num <= (1 << ERasterizerCullMode_NumBits), "ERasterizerCullMode_Num will not fit on ERasterizerCullMode_NumBits");

enum class ERasterizerDepthClipMode : uint8
{
	DepthClip,
	DepthClamp,

	Num,
	NumBits = 1,
};
static_assert(uint32(ERasterizerDepthClipMode::Num) <= (1U << uint32(ERasterizerDepthClipMode::NumBits)), "ERasterizerDepthClipMode::Num will not fit on ERasterizerDepthClipMode::NumBits");

enum EColorWriteMask
{
	CW_RED = 0x01,
	CW_GREEN = 0x02,
	CW_BLUE = 0x04,
	CW_ALPHA = 0x08,

	CW_NONE = 0,
	CW_RGB = CW_RED | CW_GREEN | CW_BLUE,
	CW_RGBA = CW_RED | CW_GREEN | CW_BLUE | CW_ALPHA,
	CW_RG = CW_RED | CW_GREEN,
	CW_BA = CW_BLUE | CW_ALPHA,

	EColorWriteMask_NumBits = 4,
};

enum ECompareFunction
{
	CF_Less,
	CF_LessEqual,
	CF_Greater,
	CF_GreaterEqual,
	CF_Equal,
	CF_NotEqual,
	CF_Never,
	CF_Always,

	ECompareFunction_Num,
	ECompareFunction_NumBits = 3,

	// Utility enumerations
	CF_DepthNearOrEqual = (((int32)ERHIZBuffer::IsInverted != 0) ? CF_GreaterEqual : CF_LessEqual),
	CF_DepthNear = (((int32)ERHIZBuffer::IsInverted != 0) ? CF_Greater : CF_Less),
	CF_DepthFartherOrEqual = (((int32)ERHIZBuffer::IsInverted != 0) ? CF_LessEqual : CF_GreaterEqual),
	CF_DepthFarther = (((int32)ERHIZBuffer::IsInverted != 0) ? CF_Less : CF_Greater),
};
static_assert(ECompareFunction_Num <= (1 << ECompareFunction_NumBits), "ECompareFunction_Num will not fit on ECompareFunction_NumBits");

enum EStencilMask
{
	SM_Default,
	SM_255,
	SM_1,
	SM_2,
	SM_4,
	SM_8,
	SM_16,
	SM_32,
	SM_64,
	SM_128,
	SM_Count
};

enum EStencilOp
{
	SO_Keep,
	SO_Zero,
	SO_Replace,
	SO_SaturatedIncrement,
	SO_SaturatedDecrement,
	SO_Invert,
	SO_Increment,
	SO_Decrement,

	EStencilOp_Num,
	EStencilOp_NumBits = 3,
};
static_assert(EStencilOp_Num <= (1 << EStencilOp_NumBits), "EStencilOp_Num will not fit on EStencilOp_NumBits");

// 340
enum EBlendOperation
{
	BO_Add,
	BO_Subtract,
	BO_Min,
	BO_Max,
	BO_ReverseSubtract,

	EBlendOperation_Num,
	EBlendOperation_NumBits = 3,
};
static_assert(EBlendOperation_Num <= (1 << EBlendOperation_NumBits), "EBlendOperation_Num will not fit on EBlendOperation_NumBits");

enum EBlendFactor
{
	BF_Zero,
	BF_One,
	BF_SourceColor,
	BF_InverseSourceColor,
	BF_SourceAlpha,
	BF_InverseSourceAlpha,
	BF_DestAlpha,
	BF_InverseDestAlpha,
	BF_DestColor,
	BF_InverseDestColor,
	BF_ConstantBlendFactor,
	BF_InverseConstantBlendFactor,
	BF_Source1Color,
	BF_InverseSource1Color,
	BF_Source1Alpha,
	BF_InverseSource1Alpha,

	EBlendFactor_Num,
	EBlendFactor_NumBits = 4,
};
static_assert(EBlendFactor_Num <= (1 << EBlendFactor_NumBits), "EBlendFactor_Num will not fit on EBlendFactor_NumBits");

// 377
enum VertexElementType
{
	VET_None,
	VET_Float1,
	VET_Float2,
	VET_Float3,
	VET_Float4,
	VET_PackedNormal, // FPackedNormal
	VET_UByte4,
	VET_UByte4N,
	VET_Color,
	VET_Short2,
	VET_Short4,
	VET_Short2N, // 16 bit word normalized to (value/32767.0,value/32767.0,0,0,1)
	VET_Half2,	 // 16 bit float using 1 bit sign, 5 bit exponent, 10 bit mantissa
	VET_Half4,
	VET_Short4N, // 4 X 16 bit word, normalized
	VET_UShort2,
	VET_UShort4,
	VET_UShort2N,  // 16 bit word normalized to (value/65535.0,value/65535.0,0,0,1)
	VET_UShort4N,  // 4 X 16 bit word unsigned, normalized
	VET_URGB10A2N, // 10 bit r, g, b and 2 bit a normalized to (value/1023.0f, value/1023.0f, value/1023.0f, value/3.0f)
	VET_UInt,
	VET_MAX,

	VET_NumBits = 5,
};
static_assert(VET_MAX <= (1 << VET_NumBits), "VET_MAX will not fit on VET_NumBits");

enum UniformBufferUsage
{
	// the uniform buffer is temporary, used for a single draw call then discarded
	UniformBuffer_SingleDraw = 0,
	// the uniform buffer is used for multiple draw calls but only for the current frame
	UniformBuffer_SingleFrame,
	// the uniform buffer is used for multiple draw calls, possibly across multiple frames
	UniformBuffer_MultiFrame,
};

enum class UniformBufferValidation
{
	None,
	ValidateResources
};

/** The base type of a value in a shader parameter structure. */
enum UniformBufferBaseType : uint8
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
	UBMT_RDG_TEXTURE_NON_PIXEL_SRV,
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

	UBMT_RESOURCE_COLLECTION,

	EUniformBufferBaseType_Num,
	EUniformBufferBaseType_NumBits = 5,
};

enum
{
	/** The maximum number of static slots allowed. */
	MAX_UNIFORM_BUFFER_STATIC_SLOTS = 255
};

/** The list of flags declaring which binding models are allowed for a uniform buffer layout. */
enum class UniformBufferBindingFlags : uint8
{
	/** If set, the uniform buffer can be bound as an RHI shader parameter on an RHI shader (i.e. RHISetShaderUniformBuffer). */
	Shader = 1 << 0,

	/** If set, the uniform buffer can be bound globally through a static slot (i.e. RHISetStaticUniformBuffers). */
	Static = 1 << 1,

	/** If set, the uniform buffer can be bound globally or per-shader, depending on the use case. Only one binding model should be
	 *  used at a time, and RHI validation will emit an error if both are used for a particular uniform buffer at the same time. This
	 *  is designed for difficult cases where a fixed single binding model would produce an unnecessary maintenance burden. Using this
	 *  disables some RHI validation errors for global bindings, so use with care.
	 */
	StaticAndShader = Static | Shader
};

// 673
enum PrimitiveType
{
	// Topology that defines a triangle N with 3 vertex extremities: 3*N+0, 3*N+1, 3*N+2.
	PT_TriangleList,

	// Topology that defines a triangle N with 3 vertex extremities: N+0, N+1, N+2.
	PT_TriangleStrip,

	// Topology that defines a line with 2 vertex extremities: 2*N+0, 2*N+1.
	PT_LineList,

	// Topology that defines a quad N with 4 vertex extremities: 4*N+0, 4*N+1, 4*N+2, 4*N+3.
	// Supported only if GRHISupportsQuadTopology == true.
	PT_QuadList,

	// Topology that defines a point N with a single vertex N.
	PT_PointList,

	// Topology that defines a screen aligned rectangle N with only 3 vertex corners:
	//    3*N + 0 is upper-left corner,
	//    3*N + 1 is upper-right corner,
	//    3*N + 2 is the lower-left corner.
	// Supported only if GRHISupportsRectTopology == true.
	PT_RectList,

	PT_Num,
	PT_NumBits = 3
};
static_assert(PT_Num <= (1 << 8), "PrimitiveType doesn't fit in a byte");
static_assert(PT_Num <= (1 << PT_NumBits), "PT_NumBits is too small");

// 704
enum EVRSAxisShadingRate : uint8
{
	VRSASR_1X = 0x0,
	VRSASR_2X = 0x1,
	VRSASR_4X = 0x2,
};

enum VRSShadingRate : uint8
{
	VRSSR_1x1 = (VRSASR_1X << 2) + VRSASR_1X,
	VRSSR_1x2 = (VRSASR_1X << 2) + VRSASR_2X,
	VRSSR_2x1 = (VRSASR_2X << 2) + VRSASR_1X,
	VRSSR_2x2 = (VRSASR_2X << 2) + VRSASR_2X,
	VRSSR_2x4 = (VRSASR_2X << 2) + VRSASR_4X,
	VRSSR_4x2 = (VRSASR_4X << 2) + VRSASR_2X,
	VRSSR_4x4 = (VRSASR_4X << 2) + VRSASR_4X,

	VRSSR_Last = VRSSR_4x4
};

enum VRSRateCombiner : uint8
{
	VRSRB_Passthrough,
	VRSRB_Override,
	VRSRB_Min,
	VRSRB_Max,
	VRSRB_Sum,
};

enum VRSImageDataType : uint8
{
	VRSImage_NotSupported, // Image-based Variable Rate Shading is not supported on the current device/platform.
	VRSImage_Palette,	   // Image-based VRS uses a palette of discrete, enumerated values to describe shading rate per tile.
	VRSImage_Fractional,   // Image-based VRS uses a floating point value to describe shading rate in X/Y (e.g. 1.0f is full rate, 0.5f is half-rate, 0.25f is 1/4 rate, etc).
};

// 740
/// Resource usage flags - for vertex and index buffers.
enum class BufferUsageFlags : uint32
{
	None = 0,

	/** The buffer will be written to once. */
	Static = 1 << 0,

	/** The buffer will be written to occasionally, GPU read only, CPU write only.  The data lifetime is until the next update, or the buffer is destroyed. */
	Dynamic = 1 << 1,

	/** The buffer's data will have a lifetime of one frame.  It MUST be written to each frame, or a new one created each frame. */
	Volatile = 1 << 2,

	/** Allows an unordered access view to be created for the buffer. */
	UnorderedAccess = 1 << 3,

	/** Create a byte address buffer, which is basically a structured buffer with a uint32 type. */
	ByteAddressBuffer = 1 << 4,

	/** Buffer that the GPU will use as a source for a copy. */
	SourceCopy = 1 << 5,

	/** Create a buffer that can be bound as a stream output target. */
	StreamOutput DEPRECATED(5.3, "StreamOut is not supported") = 1 << 6,

	/** Create a buffer which contains the arguments used by DispatchIndirect or DrawIndirect. */
	DrawIndirect = 1 << 7,

	/**
	 * Create a buffer that can be bound as a shader resource.
	 * This is only needed for buffer types which wouldn't ordinarily be used as a shader resource, like a vertex buffer.
	 */
	ShaderResource = 1 << 8,

	/** Request that this buffer is directly CPU accessible. */
	KeepCPUAccessible = 1 << 9,

	/** Buffer should go in fast vram (hint only). Requires BUF_Transient */
	FastVRAM = 1 << 10,

	/** Create a buffer that can be shared with an external RHI or process. */
	Shared = 1 << 12,

	/**
	 * Buffer contains opaque ray tracing acceleration structure data.
	 * Resources with this flag can't be bound directly to any shader stage and only can be used with ray tracing APIs.
	 * This flag is mutually exclusive with all other buffer flags except Static and ReservedResource.
	 */
	AccelerationStructure = 1 << 13,

	VertexBuffer = 1 << 14,
	IndexBuffer = 1 << 15,
	StructuredBuffer = 1 << 16,

	/** Buffer memory is allocated independently for multiple GPUs, rather than shared via driver aliasing */
	MultiGPUAllocate = 1 << 17,

	/**
	 * Tells the render graph to not bother transferring across GPUs in multi-GPU scenarios.  Useful for cases where
	 * a buffer is read back to the CPU (such as streaming request buffers), or written to each frame by CPU (such
	 * as indirect arg buffers), and the other GPU doesn't actually care about the data.
	 */
	MultiGPUGraphIgnore = 1 << 18,

	/** Allows buffer to be used as a scratch buffer for building ray tracing acceleration structure,
	 * which implies unordered access. Only changes the buffer alignment and can be combined with other flags.
	 **/
	RayTracingScratch = (1 << 19) | UnorderedAccess,

	/** The buffer is a placeholder for streaming, and does not contain an underlying GPU resource. */
	NullResource = 1 << 20,

	/** Buffer can be used as uniform buffer on platforms that do support uniform buffer objects. */
	UniformBuffer = 1 << 21,

	/**
	 * EXPERIMENTAL: Allow the buffer to be created as a reserved (AKA tiled/sparse/virtual) resource internally, without physical memory backing.
	 * May not be used with Dynamic and other buffer flags that prevent the resource from being allocated in local GPU memory.
	 */
	ReservedResource = 1 << 22,

	// Helper bit-masks
	AnyDynamic = (Dynamic | Volatile),
};
ENUM_CLASS_FLAGS(BufferUsageFlags);

#define BUF_None BufferUsageFlags::None
#define BUF_Static BufferUsageFlags::Static
#define BUF_Dynamic BufferUsageFlags::Dynamic
#define BUF_Volatile BufferUsageFlags::Volatile
#define BUF_UnorderedAccess BufferUsageFlags::UnorderedAccess
#define BUF_ByteAddressBuffer BufferUsageFlags::ByteAddressBuffer
#define BUF_SourceCopy BufferUsageFlags::SourceCopy
#define BUF_StreamOutput BufferUsageFlags::StreamOutput
#define BUF_DrawIndirect BufferUsageFlags::DrawIndirect
#define BUF_ShaderResource BufferUsageFlags::ShaderResource
#define BUF_KeepCPUAccessible BufferUsageFlags::KeepCPUAccessible
#define BUF_FastVRAM BufferUsageFlags::FastVRAM
#define BUF_Transient BufferUsageFlags::Transient
#define BUF_Shared BufferUsageFlags::Shared
#define BUF_AccelerationStructure BufferUsageFlags::AccelerationStructure
#define BUF_RayTracingScratch BufferUsageFlags::RayTracingScratch
#define BUF_VertexBuffer BufferUsageFlags::VertexBuffer
#define BUF_IndexBuffer BufferUsageFlags::IndexBuffer
#define BUF_StructuredBuffer BufferUsageFlags::StructuredBuffer
#define BUF_AnyDynamic BufferUsageFlags::AnyDynamic
#define BUF_MultiGPUAllocate BufferUsageFlags::MultiGPUAllocate
#define BUF_MultiGPUGraphIgnore BufferUsageFlags::MultiGPUGraphIgnore
#define BUF_NullResource BufferUsageFlags::NullResource
#define BUF_UniformBuffer BufferUsageFlags::UniformBuffer
#define BUF_ReservedResource BufferUsageFlags::ReservedResource

enum class ClearBinding
{
	ENoneBound,			// no clear color associated with this target.  Target will not do hardware clears on most platforms
	EColorBound,		// target has a clear color bound.  Clears will use the bound color, and do hardware clears.
	EDepthStencilBound, // target has a depthstencil value bound.  Clears will use the bound values and do hardware clears.
};

/** Maximum number of miplevels in a texture. */
enum
{
	MAX_TEXTURE_MIP_COUNT = 15
};

struct ClearValueBinding
{
	struct DSValue
	{
		float Depth;
		uint32 Stencil;
	};

	LinearColor GetClearColor() const
	{
		ensure(ColorBinding == ClearBinding::EColorBound);
		return LinearColor(Value.Color[0], Value.Color[1], Value.Color[2], Value.Color[3]);
	}

	void GetDepthStencil(float &OutDepth, uint32 &OutStencil) const
	{
		ensure(ColorBinding == ClearBinding::EDepthStencilBound);
		OutDepth = Value.DSValue.Depth;
		OutStencil = Value.DSValue.Stencil;
	}

	ClearBinding ColorBinding;

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

/**
 * Action to take when a render target is set.
 */
enum class RenderTargetLoadAction : uint8
{
	// Untouched contents of the render target are undefined. Any existing content is not preserved.
	ENoAction,

	// Existing contents are preserved.
	ELoad,

	// The render target is cleared to the fast clear value specified on the resource.
	EClear,

	Num,
	NumBits = 2,
};
static_assert((uint32)RenderTargetLoadAction::Num <= (1 << (uint32)RenderTargetLoadAction::NumBits), "ERenderTargetLoadAction::Num will not fit on ERenderTargetLoadAction::NumBits");

/**
 * Action to take when a render target is unset or at the end of a pass.
 */
enum class RenderTargetStoreAction : uint8
{
	// Contents of the render target emitted during the pass are not stored back to memory.
	ENoAction,

	// Contents of the render target emitted during the pass are stored back to memory.
	EStore,

	// Contents of the render target emitted during the pass are resolved using a box filter and stored back to memory.
	EMultisampleResolve,

	Num,
	NumBits = 2,
};
static_assert((uint32)RenderTargetStoreAction::Num <= (1 << (uint32)RenderTargetStoreAction::NumBits), "ERenderTargetStoreAction::Num will not fit on ERenderTargetStoreAction::NumBits");

/** The number of render-targets that may be simultaneously written to. */
enum
{
	MaxSimultaneousRenderTargets = 8,
	MaxSimultaneousRenderTargets_NumBits = 3,
};

// 88
enum ShaderFrequency : uint8
{
	SF_Vertex = 0,
	SF_Mesh = 1,
	SF_Amplification = 2,
	SF_Pixel = 3,
	SF_Geometry = 4,
	SF_Compute = 5,
	SF_RayGen = 6,
	SF_RayMiss = 7,
	SF_RayHitGroup = 8,
	SF_RayCallable = 9,

	SF_NumFrequencies = 10,

	// Number of standard shader frequencies for graphics pipeline (excluding compute)
	SF_NumGraphicsFrequencies = 5,

	// Number of standard shader frequencies (including compute)
	SF_NumStandardFrequencies = 6,

	SF_NumBits = 4,
};
static_assert(SF_NumFrequencies <= (1 << SF_NumBits), "SF_NumFrequencies will not fit on SF_NumBits");

// 158
/** The maximum number of vertex elements which can be used by a vertex declaration. */
enum
{
	MaxVertexElementCount = 17,
	MaxVertexElementCount_NumBits = 5,
};
static_assert(MaxVertexElementCount <= (1 << MaxVertexElementCount_NumBits), "MaxVertexElementCount will not fit on MaxVertexElementCount_NumBits");

// 636
enum ResourceLockMode
{
	RLM_ReadOnly,
	RLM_WriteOnly,
	RLM_WriteOnly_NoOverwrite,
	RLM_Num
};

/** Returns whether the shader parameter type references an RDG texture. */
inline bool IsRDGTextureReferenceShaderParameterType(UniformBufferBaseType BaseType)
{
	return BaseType == UBMT_RDG_TEXTURE || BaseType == UBMT_RDG_TEXTURE_SRV || BaseType == UBMT_RDG_TEXTURE_UAV ||
		   BaseType == UBMT_RDG_TEXTURE_ACCESS || BaseType == UBMT_RDG_TEXTURE_ACCESS_ARRAY;
}

/** Returns whether the shader parameter type references an RDG buffer. */
inline bool IsRDGBufferReferenceShaderParameterType(UniformBufferBaseType BaseType)
{
	return BaseType == UBMT_RDG_BUFFER_SRV || BaseType == UBMT_RDG_BUFFER_UAV ||
		   BaseType == UBMT_RDG_BUFFER_ACCESS || BaseType == UBMT_RDG_BUFFER_ACCESS_ARRAY;
}

/** Returns whether the shader parameter type is for RDG access and not actually for shaders. */
inline bool IsRDGResourceAccessType(UniformBufferBaseType BaseType)
{
	return BaseType == UBMT_RDG_TEXTURE_ACCESS || BaseType == UBMT_RDG_TEXTURE_ACCESS_ARRAY ||
		   BaseType == UBMT_RDG_BUFFER_ACCESS || BaseType == UBMT_RDG_BUFFER_ACCESS_ARRAY;
}

/** Returns whether the shader parameter type is a reference onto a RDG resource. */
inline bool IsRDGResourceReferenceShaderParameterType(UniformBufferBaseType BaseType)
{
	return IsRDGTextureReferenceShaderParameterType(BaseType) || IsRDGBufferReferenceShaderParameterType(BaseType) || BaseType == UBMT_RDG_UNIFORM_BUFFER;
}

/** Returns whether the shader parameter type in FRHIUniformBufferLayout is actually ignored by the RHI. */
inline bool IsShaderParameterTypeIgnoredByRHI(UniformBufferBaseType BaseType)
{
	return
		// Render targets bindings slots needs to be in FRHIUniformBufferLayout for render graph, but the RHI does not actually need to know about it.
		BaseType == UBMT_RENDER_TARGET_BINDING_SLOTS ||

		// Custom access states are used by the render graph.
		IsRDGResourceAccessType(BaseType) ||

		// #yuriy_todo: RHI is able to dereference uniform buffer in root shader parameter structures
		BaseType == UBMT_REFERENCED_STRUCT ||
		BaseType == UBMT_RDG_UNIFORM_BUFFER;
}