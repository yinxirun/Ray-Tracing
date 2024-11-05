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