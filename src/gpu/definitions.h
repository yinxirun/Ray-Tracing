#pragma once
#include <cstdint>
#include <cassert>

/*** 尹喜润 ***/
typedef uint64_t uint64;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint8_t uint8;
typedef uint16_t uint16;
#define check assert
#define ensure assert
/** NEED TO RENAME, for RT version of GFrameTime use View.ViewFamily->FrameNumber or pass down from RT from GFrameTime). */
#define GFrameNumberRenderThread 1
#define VK_DESCRIPTOR_TYPE_BEGIN_RANGE VK_DESCRIPTOR_TYPE_SAMPLER
#define VK_DESCRIPTOR_TYPE_END_RANGE VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT

/*----------------------------------------------------------------------------
    TList.
----------------------------------------------------------------------------*/

//
// Simple single-linked list template.
//
template <class ElementType>
class TList
{
public:
    ElementType Element;
    TList<ElementType> *Next;

    // Constructor.

    TList(const ElementType &InElement, TList<ElementType> *InNext = nullptr)
    {
        Element = InElement;
        Next = InNext;
    }
};
/*** 尹喜润 ***/

// Defines all bitwise operators for enum classes so it can be (mostly) used as a regular flags enum
#define ENUM_CLASS_FLAGS(Enum)                                                                                                          \
    inline Enum &operator|=(Enum &Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs | (__underlying_type(Enum))Rhs); }  \
    inline Enum &operator&=(Enum &Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs & (__underlying_type(Enum))Rhs); }  \
    inline Enum &operator^=(Enum &Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs ^ (__underlying_type(Enum))Rhs); }  \
    inline constexpr Enum operator|(Enum Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs | (__underlying_type(Enum))Rhs); } \
    inline constexpr Enum operator&(Enum Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs & (__underlying_type(Enum))Rhs); } \
    inline constexpr Enum operator^(Enum Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs ^ (__underlying_type(Enum))Rhs); } \
    inline constexpr bool operator!(Enum E) { return !(__underlying_type(Enum))E; }                                                     \
    inline constexpr Enum operator~(Enum E) { return (Enum) ~(__underlying_type(Enum))E; }

/** Flags used for texture creation */
enum class ETextureCreateFlags : uint64_t
{
    None = 0,

    // Texture can be used as a render target
    RenderTargetable = 1ull << 0,
    // Texture can be used as a resolve target
    ResolveTargetable = 1ull << 1,
    // Texture can be used as a depth-stencil target.
    DepthStencilTargetable = 1ull << 2,
    // Texture can be used as a shader resource.
    ShaderResource = 1ull << 3,
    // Texture is encoded in sRGB gamma space
    SRGB = 1ull << 4,
    // Texture data is writable by the CPU
    CPUWritable = 1ull << 5,
    // Texture will be created with an un-tiled format
    NoTiling = 1ull << 6,
    // Texture will be used for video decode
    VideoDecode = 1ull << 7,
    // Texture that may be updated every frame
    Dynamic = 1ull << 8,
    // Texture will be used as a render pass attachment that will be read from
    InputAttachmentRead = 1ull << 9,
    /** Texture represents a foveation attachment */
    Foveation = 1ull << 10,
    // Prefer 3D internal surface tiling mode for volume textures when possible
    Tiling3D = 1ull << 11,
    // This texture has no GPU or CPU backing. It only exists in tile memory on TBDR GPUs (i.e., mobile).
    Memoryless = 1ull << 12,
    // Create the texture with the flag that allows mip generation later, only applicable to D3D11
    GenerateMipCapable = 1ull << 13,
    // The texture can be partially allocated in fastvram
    FastVRAMPartialAlloc = 1ull << 14,
    // Do not create associated shader resource view, only applicable to D3D11 and D3D12
    DisableSRVCreation = 1ull << 15,
    // Do not allow Delta Color Compression (DCC) to be used with this texture
    DisableDCC = 1ull << 16,
    // UnorderedAccessView (DX11 only)
    // Warning: Causes additional synchronization between draw calls when using a render target allocated with this flag, use sparingly
    // See: GCNPerformanceTweets.pdf Tip 37
    UAV = 1ull << 17,
    // Render target texture that will be displayed on screen (back buffer)
    Presentable = 1ull << 18,
    // Texture data is accessible by the CPU
    CPUReadback = 1ull << 19,
    // Texture was processed offline (via a texture conversion process for the current platform)
    OfflineProcessed = 1ull << 20,
    // Texture needs to go in fast VRAM if available (HINT only)
    FastVRAM = 1ull << 21,
    // by default the texture is not showing up in the list - this is to reduce clutter, using the FULL option this can be ignored
    HideInVisualizeTexture = 1ull << 22,
    // Texture should be created in virtual memory, with no physical memory allocation made
    // You must make further calls to RHIVirtualTextureSetFirstMipInMemory to allocate physical memory
    // and RHIVirtualTextureSetFirstMipVisible to map the first mip visible to the GPU
    Virtual = 1ull << 23,
    // Creates a RenderTargetView for each array slice of the texture
    // Warning: if this was specified when the resource was created, you can't use SV_RenderTargetArrayIndex to route to other slices!
    TargetArraySlicesIndependently = 1ull << 24,
    // Texture that may be shared with DX9 or other devices
    Shared = 1ull << 25,
    // RenderTarget will not use full-texture fast clear functionality.
    NoFastClear = 1ull << 26,
    // Texture is a depth stencil resolve target
    DepthStencilResolveTarget = 1ull << 27,
    // Flag used to indicted this texture is a streamable 2D texture, and should be counted towards the texture streaming pool budget.
    Streamable = 1ull << 28,
    // Render target will not FinalizeFastClear; Caches and meta data will be flushed, but clearing will be skipped (avoids potentially trashing metadata)
    NoFastClearFinalize = 1ull << 29,
    /** Texture needs to support atomic operations */
    Atomic64Compatible = 1ull << 30,
    // Workaround for 128^3 volume textures getting bloated 4x due to tiling mode on some platforms.
    ReduceMemoryWithTilingMode = 1ull << 31,
    /** Texture needs to support atomic operations */
    AtomicCompatible = 1ull << 33,
    /** Texture should be allocated for external access. Vulkan only */
    External = 1ull << 34,
    /** Don't automatically transfer across GPUs in multi-GPU scenarios.  For example, if you are transferring it yourself manually. */
    MultiGPUGraphIgnore = 1ull << 35,
    /**
     * EXPERIMENTAL: Allow the texture to be created as a reserved (AKA tiled/sparse/virtual) resource internally, without physical memory backing.
     * May not be used with Dynamic and other buffer flags that prevent the resource from being allocated in local GPU memory.
     */
    ReservedResource = 1ull << 37,
    /** EXPERIMENTAL: Used with ReservedResource flag to immediately allocate and commit memory on creation. May use N small physical memory allocations instead of a single large one. */
    ImmediateCommit = 1ull << 38,

    /** Don't lump this texture with streaming memory when tracking total texture allocation sizes */
    ForceIntoNonStreamingMemoryTracking = 1ull << 39,

    /** Textures marked with this are meant to be immediately evicted after creation for intentionally crashing the GPU with a page fault. */
    Invalid = 1ull << 40,
};

ENUM_CLASS_FLAGS(ETextureCreateFlags);

// Compatibility defines
#define TexCreate_None ETextureCreateFlags::None
#define TexCreate_RenderTargetable ETextureCreateFlags::RenderTargetable
#define TexCreate_ResolveTargetable ETextureCreateFlags::ResolveTargetable
#define TexCreate_DepthStencilTargetable ETextureCreateFlags::DepthStencilTargetable
#define TexCreate_ShaderResource ETextureCreateFlags::ShaderResource
#define TexCreate_SRGB ETextureCreateFlags::SRGB
#define TexCreate_CPUWritable ETextureCreateFlags::CPUWritable
#define TexCreate_NoTiling ETextureCreateFlags::NoTiling
#define TexCreate_VideoDecode ETextureCreateFlags::VideoDecode
#define TexCreate_Dynamic ETextureCreateFlags::Dynamic
#define TexCreate_InputAttachmentRead ETextureCreateFlags::InputAttachmentRead
#define TexCreate_Foveation ETextureCreateFlags::Foveation
#define TexCreate_3DTiling ETextureCreateFlags::Tiling3D
#define TexCreate_Memoryless ETextureCreateFlags::Memoryless
#define TexCreate_GenerateMipCapable ETextureCreateFlags::GenerateMipCapable
#define TexCreate_FastVRAMPartialAlloc ETextureCreateFlags::FastVRAMPartialAlloc
#define TexCreate_DisableSRVCreation ETextureCreateFlags::DisableSRVCreation
#define TexCreate_DisableDCC ETextureCreateFlags::DisableDCC
#define TexCreate_UAV ETextureCreateFlags::UAV
#define TexCreate_Presentable ETextureCreateFlags::Presentable
#define TexCreate_CPUReadback ETextureCreateFlags::CPUReadback
#define TexCreate_OfflineProcessed ETextureCreateFlags::OfflineProcessed
#define TexCreate_FastVRAM ETextureCreateFlags::FastVRAM
#define TexCreate_HideInVisualizeTexture ETextureCreateFlags::HideInVisualizeTexture
#define TexCreate_Virtual ETextureCreateFlags::Virtual
#define TexCreate_TargetArraySlicesIndependently ETextureCreateFlags::TargetArraySlicesIndependently
#define TexCreate_Shared ETextureCreateFlags::Shared
#define TexCreate_NoFastClear ETextureCreateFlags::NoFastClear
#define TexCreate_DepthStencilResolveTarget ETextureCreateFlags::DepthStencilResolveTarget
#define TexCreate_Streamable ETextureCreateFlags::Streamable
#define TexCreate_NoFastClearFinalize ETextureCreateFlags::NoFastClearFinalize
#define TexCreate_ReduceMemoryWithTilingMode ETextureCreateFlags::ReduceMemoryWithTilingMode
#define TexCreate_Transient ETextureCreateFlags::Transient
#define TexCreate_AtomicCompatible ETextureCreateFlags::AtomicCompatible
#define TexCreate_External ETextureCreateFlags::External
#define TexCreate_MultiGPUGraphIgnore ETextureCreateFlags::MultiGPUGraphIgnore
#define TexCreate_ReservedResource ETextureCreateFlags::ReservedResource
#define TexCreate_ImmediateCommit ETextureCreateFlags::ImmediateCommit
#define TexCreate_Invalid ETextureCreateFlags::Invalid
