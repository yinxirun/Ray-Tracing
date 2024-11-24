#pragma once
#include "Volk/volk.h"
#include "core/templates/alignment_template.h"
#include "core/assertion_macros.h"
#include "core/math/color.h"
#include "core/math/vec.h"
#include "definitions.h"
#include "core/pixel_format.h"
#include "core/misc/secure_hash.h"
#include "RHIDefinitions.h"
#include "RHIAccess.h"
#include "RHIUtilities.h"
#include "RHIResources.h"
#include "RHI.h"
#include "RHIImmutableSamplerState.h"
#include <array>
#include <vector>
#include <atomic>
#include <memory>

typedef std::vector<VertexElement> VertexDeclarationElementList;

struct UniformBufferResource;

// 222
struct ClearValueBinding
{
    struct DSValue
    {
        float Depth;
        uint32 Stencil;
    };

    ClearValueBinding() : ColorBinding(ClearBinding::EColorBound)
    {
        Value.Color[0] = 0.0f;
        Value.Color[1] = 0.0f;
        Value.Color[2] = 0.0f;
        Value.Color[3] = 0.0f;
    }

    ClearValueBinding(ClearBinding NoBinding) : ColorBinding(NoBinding)
    {
        check(ColorBinding == ClearBinding::ENoneBound);
        Value.Color[0] = 0.0f;
        Value.Color[1] = 0.0f;
        Value.Color[2] = 0.0f;
        Value.Color[3] = 0.0f;
        Value.DSValue.Depth = 0.0f;
        Value.DSValue.Stencil = 0;
    }

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

    // common clear values
    static const ClearValueBinding None;
};

// 351
struct ResourceCreateInfo
{
    // for CreateTexture calls
    void *BulkData;

    // for CreateBuffer calls
    void *ResourceArray = 0;
};

/** Descriptor used to create a texture resource */
struct TextureDesc
{
    TextureDesc() = default;
    TextureDesc(TextureDimension InDimension) : Dimension(InDimension) {}

    /** Texture dimension to use when creating the RHI texture. */
    TextureDimension Dimension = TextureDimension::Texture2D;

    /** Pixel format used to create RHI texture. */
    PixelFormat Format = PixelFormat::PF_Unknown;

    /** Clear value to use when fast-clearing the texture. */
    ClearValueBinding ClearValue;

    /** Extent of the texture in x and y. */
    IntVec2 Extent = {1, 1};

    /** Depth of the texture if the dimension is 3D. */
    uint16 Depth = 1;

    /** The number of array elements in the texture. (Keep at 1 if dimension is 3D). */
    uint16 ArraySize = 1;

    /** Number of mips in the texture mip-map chain. */
    uint8 NumMips = 1;

    /** Number of samples in the texture. >1 for MSAA. */
    uint8 NumSamples = 1;

    /** Texture flags passed on to RHI texture. */
    TextureCreateFlags Flags = TexCreate_None;
};

struct TextureCreateDesc : public TextureDesc
{
    static TextureCreateDesc Create2D(const char *InDebugName)
    {
        return TextureCreateDesc(InDebugName, TextureDimension::Texture2D);
    }

    static TextureCreateDesc Create2D(const char *DebugName, int32 SizeX, int32 SizeY, PixelFormat Format)
    {
        return Create2D(DebugName).SetExtent(SizeX, SizeY).SetFormat(Format);
    }

    TextureCreateDesc() = default;

    // Constructor with minimal argument set. Name and dimension are always required.
    TextureCreateDesc(const char *InDebugName, TextureDimension InDimension) : TextureDesc(InDimension), DebugName(InDebugName) {}

    TextureCreateDesc &SetExtent(int32 InExtentX, int32 InExtentY)
    {
        Extent.x = InExtentX;
        Extent.y = InExtentY;
        return *this;
    }
    TextureCreateDesc &SetFormat(PixelFormat InFormat)
    {
        Format = InFormat;
        return *this;
    }
    TextureCreateDesc &SetFlags(TextureCreateFlags InFlags)
    {
        Flags = InFlags;
        return *this;
    }
    TextureCreateDesc &SetClearValue(ClearValueBinding InClearValue)
    {
        ClearValue = InClearValue;
        return *this;
    }
    TextureCreateDesc &SetInitialState(Access InInitialState)
    {
        InitialState = InInitialState;
        return *this;
    }
    TextureCreateDesc &DetermineInititialState()
    {
        if (InitialState == Access::Unknown)
            InitialState = RHIGetDefaultResourceState(Flags, BulkData != nullptr);
        return *this;
    }

    /* The RHI access state that the resource will be created in. */
    Access InitialState = Access::Unknown;

    /* A friendly name for the resource. */
    const char *DebugName = nullptr;

    uint8 *BulkData = nullptr;
    size_t BulkDataSize = 0;
};

/// @note 在UE中，想要用TRefCountPointer 管理资源，该资源类型就必须实现 AddRef 和 Release 。但是在这里，我使用std::shared_ptr管理，好像两个方法没什么用处
class RHIResource
{
public:
    RHIResource(ERHIResourceType InResourceType = RRT_None);

    virtual ~RHIResource();

    __forceinline uint32 AddRef()
    {
        numRefs.fetch_add(1);
        uint32_t newValue = numRefs.load();
        assert(newValue > 0);
        return (uint32_t)newValue;
    }

    __forceinline uint32 Release()
    {
        numRefs.fetch_sub(1);
        uint32_t newValue = numRefs.load();
        if (newValue == 0)
        {
            delete this;
        }
        assert(newValue >= 0);
        return (uint32)newValue;
    }

private:
    std::atomic<uint32> numRefs{0};
    const ERHIResourceType ResourceType;
};

class RHIViewableResource : public RHIResource
{
public:
    RHIViewableResource(ERHIResourceType InResourceType, Access InAccess);

    void ReleaseOwnership() { TrackedAccess = Access::Unknown; }

private:
    Access TrackedAccess;
};

// 740
class BoundShaderState : public RHIResource
{
public:
    BoundShaderState() : RHIResource(RRT_BoundShaderState) {}
};

// 993
class GraphicsPipelineState : public RHIResource
{
public:
    GraphicsPipelineState() : RHIResource(RRT_GraphicsPipelineState) {}

    inline void SetSortKey(uint64 InSortKey) { SortKey = InSortKey; }
    inline uint64 GetSortKey() const { return SortKey; }

private:
    uint64 SortKey = 0;
};

// 1189
struct BufferDesc
{
    uint32 Size{};
    uint32 Stride{};
    BufferUsageFlags Usage{};

    BufferDesc() = default;
    BufferDesc(uint32 InSize, uint32 InStride, BufferUsageFlags InUsage)
        : Size(InSize), Stride(InStride), Usage(InUsage)
    {
    }

    static BufferDesc Null()
    {
        return BufferDesc(0, 0, BUF_NullResource);
    }

    bool IsNull() const
    {
        if (EnumHasAnyFlags(Usage, BUF_NullResource))
        {
            // The null resource descriptor should have its other fields zeroed, and no additional flags.
            check(Size == 0 && Stride == 0 && Usage == BUF_NullResource);
            return true;
        }

        return false;
    }
};

class Texture : public RHIViewableResource
{
protected:
    Texture(const TextureCreateDesc &InDesc);

public:
    /**
     * Get the texture description used to create the texture
     * Still virtual because FRHITextureReference can override this function - remove virtual when FRHITextureReference is deprecated
     *
     * @return TextureDesc used to create the texture
     */
    virtual const TextureDesc &GetDesc() const { return textureDesc; }

    /// Returns access to the platform-specific RHI texture baseclass.  This is designed to provide the RHI with fast access to its base classes in the face of multiple inheritance.
    /// @return	The pointer to the platform-specific RHI texture baseclass or NULL if it not initialized or not supported for this RHI
    virtual void *GetTextureBaseRHI() { return nullptr; }

    IntVec3 GetMipDimensions(uint8 MipIndex) const
    {
        const TextureDesc &Desc = GetDesc();
        return IntVec3(
            std::max<int32>(Desc.Extent.x >> MipIndex, 1),
            std::max<int32>(Desc.Extent.y >> MipIndex, 1),
            std::max<int32>(Desc.Depth >> MipIndex, 1));
    }

    /** @return Whether the texture is multi sampled. */
    bool IsMultisampled() const { return GetDesc().NumSamples > 1; }

    /** @return Whether the texture has a clear color defined */
    bool HasClearValue() const
    {
        return GetDesc().ClearValue.ColorBinding != ClearBinding::ENoneBound;
    }

    /** @return the clear color value if set */
    LinearColor GetClearColor() const { return GetDesc().ClearValue.GetClearColor(); }

    /** @return the depth & stencil clear value if set */
    void GetDepthStencilClearValue(float &OutDepth, uint32 &OutStencil) const
    {
        return GetDesc().ClearValue.GetDepthStencil(OutDepth, OutStencil);
    }

private:
    TextureDesc textureDesc;
};

// 416
class ExclusiveDepthStencil
{
public:
    enum Type
    {
        // don't use those directly, use the combined versions below
        // 4 bits are used for depth and 4 for stencil to make the hex value readable and non overlapping
        DepthNop = 0x00,
        DepthRead = 0x01,
        DepthWrite = 0x02,
        DepthMask = 0x0f,
        StencilNop = 0x00,
        StencilRead = 0x10,
        StencilWrite = 0x20,
        StencilMask = 0xf0,

        // use those:
        DepthNop_StencilNop = DepthNop + StencilNop,
        DepthRead_StencilNop = DepthRead + StencilNop,
        DepthWrite_StencilNop = DepthWrite + StencilNop,
        DepthNop_StencilRead = DepthNop + StencilRead,
        DepthRead_StencilRead = DepthRead + StencilRead,
        DepthWrite_StencilRead = DepthWrite + StencilRead,
        DepthNop_StencilWrite = DepthNop + StencilWrite,
        DepthRead_StencilWrite = DepthRead + StencilWrite,
        DepthWrite_StencilWrite = DepthWrite + StencilWrite,
    };

private:
    Type Value;

public:
    // constructor
    ExclusiveDepthStencil(Type InValue = DepthNop_StencilNop) : Value(InValue) {}

    inline bool IsDepthRead() const
    {
        return ExtractDepth() == DepthRead;
    }
    inline bool IsStencilRead() const
    {
        return ExtractStencil() == StencilRead;
    }
    inline bool IsDepthWrite() const
    {
        return ExtractDepth() == DepthWrite;
    }
    inline bool IsStencilWrite() const
    {
        return ExtractStencil() == StencilWrite;
    }

private:
    inline Type ExtractDepth() const { return (Type)(Value & DepthMask); }
    inline Type ExtractStencil() const { return (Type)(Value & StencilMask); }
};

// 606
static inline VkAttachmentLoadOp RenderTargetLoadActionToVulkan(RenderTargetLoadAction InLoadAction)
{
    VkAttachmentLoadOp OutLoadAction = VK_ATTACHMENT_LOAD_OP_MAX_ENUM;

    switch (InLoadAction)
    {
    case RenderTargetLoadAction::ELoad:
        OutLoadAction = VK_ATTACHMENT_LOAD_OP_LOAD;
        break;
    case RenderTargetLoadAction::EClear:
        OutLoadAction = VK_ATTACHMENT_LOAD_OP_CLEAR;
        break;
    case RenderTargetLoadAction::ENoAction:
        OutLoadAction = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        break;
    default:
        break;
    }

    // Check for missing translation
    check(OutLoadAction != VK_ATTACHMENT_LOAD_OP_MAX_ENUM);
    return OutLoadAction;
}
static inline VkAttachmentStoreOp RenderTargetStoreActionToVulkan(RenderTargetStoreAction InStoreAction)
{
    VkAttachmentStoreOp OutStoreAction = VK_ATTACHMENT_STORE_OP_MAX_ENUM;

    switch (InStoreAction)
    {
    case RenderTargetStoreAction::EStore:
        OutStoreAction = VK_ATTACHMENT_STORE_OP_STORE;
        break;
    case RenderTargetStoreAction::ENoAction:
    case RenderTargetStoreAction::EMultisampleResolve:
        OutStoreAction = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        break;
    default:
        break;
    }

    // Check for missing translation
    check(OutStoreAction != VK_ATTACHMENT_STORE_OP_MAX_ENUM);
    return OutStoreAction;
}

// 678
//
// State blocks
//
class SamplerState : public RHIResource
{
public:
    SamplerState() : RHIResource(RRT_SamplerState) {}
    virtual bool IsImmutable() const { return false; }
    virtual RHIDescriptorHandle GetBindlessHandle() const { return RHIDescriptorHandle(); }
};

class RasterizerState : public RHIResource
{
public:
    RasterizerState() : RHIResource(RRT_RasterizerState) {}
    virtual bool GetInitializer(struct RasterizerStateInitializer &Init) { return false; }
};

class DepthStencilState : public RHIResource
{
public:
    DepthStencilState() : RHIResource(RRT_DepthStencilState) {}
    virtual bool GetInitializer(struct DepthStencilStateInitializer &Init) { return false; }
};

class BlendState : public RHIResource
{
public:
    BlendState() : RHIResource(RRT_BlendState) {}
    virtual bool GetInitializer(class BlendStateInitializerRHI &Init) { return false; }
};

// 753
class VertexDeclaration : public RHIResource
{
public:
    VertexDeclaration() : RHIResource(RRT_VertexDeclaration) {}
    virtual bool GetInitializer(VertexDeclarationElementList &Init) { return false; }
    virtual uint32 GetPrecachePSOHash() const { return 0; }
};

// 762
class RHIShader : public RHIResource
{
public:
public:
    void SetHash(SHAHash InHash) { Hash = InHash; }
    SHAHash GetHash() const { return Hash; }

    RHIShader(ERHIResourceType InResourceType, ShaderFrequency InFrequency)
        : RHIResource(InResourceType), Frequency(InFrequency)
    // , bNoDerivativeOps(false)
    // , bHasShaderBundleUsage(false)
    {
    }

    inline ShaderFrequency GetFrequency() const { return Frequency; }

private:
    SHAHash Hash;
    ShaderFrequency Frequency;
};

class GraphicsShader : public RHIShader
{
public:
    explicit GraphicsShader(ERHIResourceType InResourceType, ShaderFrequency InFrequency)
        : RHIShader(InResourceType, InFrequency) {}
};

/// 由shader factory负责回收
class VertexShader : public GraphicsShader
{
public:
    VertexShader() : GraphicsShader(RRT_VertexShader, SF_Vertex) {}
};

class RHIMeshShader : public GraphicsShader
{
public:
    RHIMeshShader() : GraphicsShader(RRT_MeshShader, SF_Mesh) {}
};

/// 由shader factory负责回收
class PixelShader : public GraphicsShader
{
public:
    PixelShader() : GraphicsShader(RRT_PixelShader, SF_Pixel) {}
};

class RHIGeometryShader : public GraphicsShader
{
public:
    RHIGeometryShader() : GraphicsShader(RRT_GeometryShader, SF_Geometry) {}
};

class RHIComputeShader : public RHIShader
{
public:
    RHIComputeShader() : RHIShader(RRT_ComputeShader, SF_Compute) //, Stats(nullptr)
    {
    }

    // inline void SetStats(struct FPipelineStateStats *Ptr) { Stats = Ptr; }
    //  void UpdateStats();

private:
    // struct FPipelineStateStats *Stats;
};

//
// Buffer
//

/** Data structure to store information about resource parameter in a shader parameter structure. */
struct UniformBufferResource
{
    /** Byte offset to each resource in the uniform buffer memory. */
    uint16 memberOffset;

    /** Type of the member that allow (). */
    UniformBufferBaseType memberType;

    /** Compare two uniform buffer layout resources. */
    friend inline bool operator==(const UniformBufferResource &A, const UniformBufferResource &B)
    {
        return A.memberOffset == B.memberOffset && A.memberType == B.memberType;
    }
};

/** Initializer for the layout of a uniform buffer in memory. */
struct UniformBufferLayoutInitializer
{
public:
    UniformBufferLayoutInitializer() = default;

    inline uint32 GetHash() const
    {
        check(Hash != 0);
        return Hash;
    }

    void ComputeHash()
    {
        // Static slot is not stable. Just track whether we have one at all.
        uint32 TmpHash = ConstantBufferSize << 16 | static_cast<uint32>(BindingFlags) << 8 | static_cast<uint32>(StaticSlot != MAX_UNIFORM_BUFFER_STATIC_SLOTS);

        for (int32 ResourceIndex = 0; ResourceIndex < Resources.size(); ResourceIndex++)
        {
            // Offset and therefore hash must be the same regardless of pointer size
            check(Resources[ResourceIndex].memberOffset == Align(Resources[ResourceIndex].memberOffset, SHADER_PARAMETER_POINTER_ALIGNMENT));
            TmpHash ^= Resources[ResourceIndex].memberOffset;
        }

        uint32 N = Resources.size();
        while (N >= 4)
        {
            TmpHash ^= (Resources[--N].memberType << 0);
            TmpHash ^= (Resources[--N].memberType << 8);
            TmpHash ^= (Resources[--N].memberType << 16);
            TmpHash ^= (Resources[--N].memberType << 24);
        }
        while (N >= 2)
        {
            TmpHash ^= Resources[--N].memberType << 0;
            TmpHash ^= Resources[--N].memberType << 16;
        }
        while (N > 0)
        {
            TmpHash ^= Resources[--N].memberType;
        }
        Hash = TmpHash;
    }

    std::vector<UniformBufferResource> Resources;
    uint32 ConstantBufferSize = 0;
    uint8 StaticSlot = MAX_UNIFORM_BUFFER_STATIC_SLOTS;
    UniformBufferBindingFlags BindingFlags = UniformBufferBindingFlags::Shader;

private:
    uint32 Hash = 0;
};

/** The layout of a uniform buffer in memory. */
struct UniformBufferLayout : public RHIResource
{
    UniformBufferLayout() = delete;

    explicit UniformBufferLayout(const UniformBufferLayoutInitializer &Initializer)
        : RHIResource(RRT_UniformBufferLayout),
          Resources(Initializer.Resources), Hash(Initializer.GetHash()), ConstantBufferSize(Initializer.ConstantBufferSize),
          StaticSlot(Initializer.StaticSlot), BindingFlags(Initializer.BindingFlags)
    {
    }

    inline uint32 GetHash() const
    {
        check(Hash != 0);
        return Hash;
    }

    /** The list of all resource inlined into the shader parameter structure. */
    const std::vector<UniformBufferResource> Resources;

    const uint32 Hash;

    /** The size of the constant buffer in bytes. */
    const uint32 ConstantBufferSize;

    /** The static slot (if applicable). */
    const uint8 StaticSlot;

    /** The binding flags describing how this resource can be bound to the RHI. */
    const UniformBufferBindingFlags BindingFlags;
};

class UniformBuffer : public RHIResource
{
public:
    UniformBuffer() = delete;

    /** Initialization constructor. */
    UniformBuffer(std::shared_ptr<const UniformBufferLayout> InLayout)
        : RHIResource(RRT_UniformBuffer), Layout(InLayout), LayoutConstantBufferSize(InLayout->ConstantBufferSize)
    {
    }

    /** @return The number of bytes in the uniform buffer. */
    uint32 GetSize() const
    {
        check(LayoutConstantBufferSize == Layout->ConstantBufferSize);
        return LayoutConstantBufferSize;
    }
    const UniformBufferLayout &GetLayout() const { return *Layout; }
    const UniformBufferLayout *GetLayoutPtr() const { return Layout.get(); }

protected:
    std::vector<RHIResource *> ResourceTable;

private:
    /** Layout of the uniform buffer. */
    std::shared_ptr<const UniformBufferLayout> Layout;
    uint32 LayoutConstantBufferSize;
};

// 1220
class Buffer : public RHIViewableResource
{
public:
    /** Initialization constructor. */
    Buffer(BufferDesc const &InDesc)
        : RHIViewableResource(RRT_Buffer, Access::Unknown /* TODO (RemoveUnknowns): Use InitialAccess from descriptor after refactor. */),
          Desc(InDesc)
    {
    }

    BufferDesc const &GetDesc() const { return Desc; }

    /** @return The number of bytes in the buffer. */
    uint32 GetSize() const { return Desc.Size; }
    /** @return The stride in bytes of the buffer. */
    uint32 GetStride() const { return Desc.Stride; }
    /** @return The usage flags used to create the buffer. */
    BufferUsageFlags GetUsage() const { return Desc.Usage; }

protected:
    void ReleaseOwnership()
    {
        RHIViewableResource::ReleaseOwnership();
        Desc = BufferDesc::Null();
    }

private:
    BufferDesc Desc;
};

// 1918
/*
 * Generic GPU fence class.
 * Granularity differs depending on backing RHI - ie it may only represent command buffer granularity.
 * RHI specific fences derive from this to implement real GPU->CPU fencing.
 * The default implementation always returns false for Poll until the next frame from the frame the fence was inserted
 * because not all APIs have a GPU/CPU sync object, we need to fake it.
 */
class GPUFence : public RHIResource
{
public:
    GPUFence() : RHIResource(RRT_GPUFence) {}
    virtual ~GPUFence() {}

    virtual void Clear() = 0;

    /**
     * Poll the fence to see if the GPU has signaled it.
     * @returns True if and only if the GPU fence has been inserted and the GPU has signaled the fence.
     */
    virtual bool Poll() const = 0;
};

class Viewport : public RHIResource
{
public:
    Viewport() : RHIResource(RRT_Viewport) {}
};

// 3214
class RHIRenderTargetView
{
public:
    Texture *texture = nullptr;
    uint32 MipIndex = 0;

    /** Array slice or texture cube face.  Only valid if texture resource was created with TexCreate_TargetArraySlicesIndependently! */
    uint32 ArraySliceIndex = -1;

    RenderTargetLoadAction LoadAction = RenderTargetLoadAction::ENoAction;
    RenderTargetStoreAction StoreAction = RenderTargetStoreAction::ENoAction;
};
class RHIDepthRenderTargetView
{
public:
    Texture *texture;

    RenderTargetLoadAction DepthLoadAction;
    RenderTargetStoreAction DepthStoreAction;
    RenderTargetLoadAction StencilLoadAction;

private:
    RenderTargetStoreAction StencilStoreAction;
    ExclusiveDepthStencil DepthStencilAccess;

public:
    explicit RHIDepthRenderTargetView() : texture(nullptr),
                                          DepthLoadAction(RenderTargetLoadAction::ENoAction),
                                          DepthStoreAction(RenderTargetStoreAction::ENoAction),
                                          StencilLoadAction(RenderTargetLoadAction::ENoAction),
                                          StencilStoreAction(RenderTargetStoreAction::ENoAction),
                                          DepthStencilAccess(ExclusiveDepthStencil::DepthNop_StencilNop)
    {
        Validate();
    }
    explicit RHIDepthRenderTargetView(Texture *InTexture, RenderTargetLoadAction InDepthLoadAction,
                                      RenderTargetStoreAction InDepthStoreAction,
                                      RenderTargetLoadAction InStencilLoadAction,
                                      RenderTargetStoreAction InStencilStoreAction,
                                      ExclusiveDepthStencil InDepthStencilAccess)
        : texture(InTexture),
          DepthLoadAction(InDepthLoadAction),
          DepthStoreAction(InDepthStoreAction),
          StencilLoadAction(InStencilLoadAction),
          StencilStoreAction(InStencilStoreAction),
          DepthStencilAccess(InDepthStencilAccess)
    {
        Validate();
    }
    void Validate() const
    {
        // VK and Metal MAY leave the attachment in an undefined state if the StoreAction is DontCare. So we can't assume read-only implies it should be DontCare unless we know for sure it will never be used again.
        // ensureMsgf(DepthStencilAccess.IsDepthWrite() || DepthStoreAction == ERenderTargetStoreAction::ENoAction, TEXT("Depth is read-only, but we are performing a store.  This is a waste on mobile.  If depth can't change, we don't need to store it out again"));
        /*ensureMsgf(DepthStencilAccess.IsStencilWrite() || StencilStoreAction == ERenderTargetStoreAction::ENoAction, TEXT("Stencil is read-only, but we are performing a store.  This is a waste on mobile.  If stencil can't change, we don't need to store it out again"));*/
    }
};

// 3363
class SetRenderTargetsInfo
{
public:
    // Color Render Targets Info
    RHIRenderTargetView ColorRenderTarget[MaxSimultaneousRenderTargets];
    int32 NumColorRenderTargets;
    bool bClearColor;

    // Color Render Targets Info
    RHIRenderTargetView ColorResolveRenderTarget[MaxSimultaneousRenderTargets];
    bool bHasResolveAttachments;

    // Depth/Stencil Render Target Info
    RHIDepthRenderTargetView DepthStencilRenderTarget;
    bool bClearDepth;
    bool bClearStencil;

    Texture *ShadingRateTexture;
    // EVRSRateCombiner ShadingRateTextureCombiner;

    uint8 MultiViewCount;

    SetRenderTargetsInfo() : NumColorRenderTargets(0), bClearColor(false), bHasResolveAttachments(false),
                             bClearDepth(false), ShadingRateTexture(nullptr), MultiViewCount(0) {}
};

// 3551
struct BoundShaderStateInput
{
    inline BoundShaderStateInput() {}

    inline BoundShaderStateInput(
        VertexDeclaration *InVertexDeclarationRHI, VertexShader *InVertexShaderRHI, PixelShader *InPixelShaderRHI
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
        ,
        FRHIGeometryShader *InGeometryShaderRHI
#endif
        )
        : VertexDeclarationRHI(InVertexDeclarationRHI), VertexShaderRHI(InVertexShaderRHI), PixelShaderRHI(InPixelShaderRHI)
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
          ,
          GeometryShaderRHI(InGeometryShaderRHI)
#endif
    {
    }

#if PLATFORM_SUPPORTS_MESH_SHADERS
    inline FBoundShaderStateInput(
        FRHIMeshShader *InMeshShaderRHI,
        FRHIAmplificationShader *InAmplificationShader,
        FRHIPixelShader *InPixelShaderRHI)
        : PixelShaderRHI(InPixelShaderRHI), MeshShaderRHI(InMeshShaderRHI), AmplificationShaderRHI(InAmplificationShader)
    {
    }
#endif

    VertexShader *GetVertexShader() const { return VertexShaderRHI; }
    PixelShader *GetPixelShader() const { return PixelShaderRHI; }

#if PLATFORM_SUPPORTS_MESH_SHADERS
    FRHIMeshShader *GetMeshShader() const { return MeshShaderRHI; }
    void SetMeshShader(FRHIMeshShader *InMeshShader) { MeshShaderRHI = InMeshShader; }
    FRHIAmplificationShader *GetAmplificationShader() const { return AmplificationShaderRHI; }
    void SetAmplificationShader(FRHIAmplificationShader *InAmplificationShader) { AmplificationShaderRHI = InAmplificationShader; }
#else
    constexpr RHIMeshShader *GetMeshShader() const { return nullptr; }
    void SetMeshShader(RHIMeshShader *) {}
#endif

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    FRHIGeometryShader *GetGeometryShader() const { return GeometryShaderRHI; }
    void SetGeometryShader(FRHIGeometryShader *InGeometryShader) { GeometryShaderRHI = InGeometryShader; }
#else
    constexpr RHIGeometryShader *GetGeometryShader() const { return nullptr; }
    void SetGeometryShader(RHIGeometryShader *) {}
#endif

    VertexDeclaration *VertexDeclarationRHI = nullptr;
    VertexShader *VertexShaderRHI = nullptr;
    PixelShader *PixelShaderRHI = nullptr;

private:
#if PLATFORM_SUPPORTS_MESH_SHADERS
    FRHIMeshShader *MeshShaderRHI = nullptr;
    FRHIAmplificationShader *AmplificationShaderRHI = nullptr;
#endif
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    FRHIGeometryShader *GeometryShaderRHI = nullptr;
#endif
};

// 3687
// Hints for some RHIs that support subpasses
enum class SubpassHint : uint8
{
    // Regular rendering
    None,
    // Render pass has depth reading subpass
    DepthReadSubpass,
    // Mobile defferred shading subpass
    DeferredShadingSubpass,
    // Mobile MSAA custom resolve subpass. Includes DepthReadSubpass.
    CustomResolveSubpass,
};

// 3703
enum class ConservativeRasterization : uint8
{
    Disabled,
    Overestimated,
};

// 3734
class GraphicsPipelineStateInitializer
{
public:
    // Can't use TEnumByte<EPixelFormat> as it changes the struct to be non trivially constructible, breaking memset
    using TRenderTargetFormats = std::array<uint8 /*EPixelFormat*/, MaxSimultaneousRenderTargets>;
    using TRenderTargetFlags = std::array<TextureCreateFlags, MaxSimultaneousRenderTargets>;

    GraphicsPipelineStateInitializer()
        : BlendState(nullptr), RasterizerState(nullptr), DepthStencilState(nullptr),
          RenderTargetsEnabled(0), DepthStencilTargetFormat(PF_Unknown), DepthStencilTargetFlag(TexCreate_None),
          DepthTargetLoadAction(RenderTargetLoadAction::ENoAction), DepthTargetStoreAction(RenderTargetStoreAction::ENoAction),
          StencilTargetLoadAction(RenderTargetLoadAction::ENoAction), StencilTargetStoreAction(RenderTargetStoreAction::ENoAction),
          NumSamples(0), SubpassHint(SubpassHint::None), SubpassIndex(0), ConservativeRasterization(ConservativeRasterization::Disabled), bDepthBounds(false), MultiViewCount(0), bHasFragmentDensityAttachment(false),
          bAllowVariableRateShading(false),
          ShadingRate(VRSShadingRate::VRSSR_1x1), Flags(0),
          StatePrecachePSOHash(0)
    {
        RenderTargetFormats.fill(static_cast<uint8>(PF_Unknown));
        RenderTargetFlags.fill(TexCreate_None);
    }

    // We care about flags that influence RT formats (which is the only thing the underlying API cares about).
    // In most RHIs, the format is only influenced by TexCreate_SRGB. D3D12 additionally uses TexCreate_Shared in its format selection logic.
    static constexpr TextureCreateFlags RelevantRenderTargetFlagMask = TextureCreateFlags::SRGB | TextureCreateFlags::Shared;

    // We care about flags that influence DS formats (which is the only thing the underlying API cares about).
    // D3D12 shares the format choice function with the RT, so preserving all the flags used there out of abundance of caution.
    static constexpr TextureCreateFlags RelevantDepthStencilFlagMask = TextureCreateFlags::SRGB | TextureCreateFlags::Shared | TextureCreateFlags::DepthStencilTargetable;

    // 	static bool RelevantRenderTargetFlagsEqual(const TRenderTargetFlags& A, const TRenderTargetFlags& B)
    // 	{
    // 		for (int32 Index = 0; Index < A.Num(); ++Index)
    // 		{
    // 			ETextureCreateFlags FlagsA = A[Index] & RelevantRenderTargetFlagMask;
    // 			ETextureCreateFlags FlagsB = B[Index] & RelevantRenderTargetFlagMask;
    // 			if (FlagsA != FlagsB)
    // 			{
    // 				return false;
    // 			}
    // 		}
    // 		return true;
    // 	}

    // 	static bool RelevantDepthStencilFlagsEqual(const ETextureCreateFlags A, const ETextureCreateFlags B)
    // 	{
    // 		ETextureCreateFlags FlagsA = (A & RelevantDepthStencilFlagMask);
    // 		ETextureCreateFlags FlagsB = (B & RelevantDepthStencilFlagMask);
    // 		return (FlagsA == FlagsB);
    // 	}

    uint32 ComputeNumValidRenderTargets() const
    {
        // Get the count of valid render targets (ignore those at the end of the array with PF_Unknown)
        if (RenderTargetsEnabled > 0)
        {
            int32 LastValidTarget = -1;
            for (int32 i = (int32)RenderTargetsEnabled - 1; i >= 0; i--)
            {
                if (RenderTargetFormats[i] != PF_Unknown)
                {
                    LastValidTarget = i;
                    break;
                }
            }
            return uint32(LastValidTarget + 1);
        }
        return RenderTargetsEnabled;
    }

    BoundShaderStateInput BoundShaderState;
    BlendState *BlendState;
    RasterizerState *RasterizerState;
    DepthStencilState *DepthStencilState;
    ImmutableSamplerState ImmutableSamplerState;

    PrimitiveType PrimitiveType;
    uint32 RenderTargetsEnabled;
    TRenderTargetFormats RenderTargetFormats;
    TRenderTargetFlags RenderTargetFlags;
    PixelFormat DepthStencilTargetFormat;
    TextureCreateFlags DepthStencilTargetFlag;
    RenderTargetLoadAction DepthTargetLoadAction;
    RenderTargetStoreAction DepthTargetStoreAction;
    RenderTargetLoadAction StencilTargetLoadAction;
    RenderTargetStoreAction StencilTargetStoreAction;
    ExclusiveDepthStencil DepthStencilAccess;
    uint16 NumSamples;
    SubpassHint SubpassHint;
    uint8 SubpassIndex;
    ConservativeRasterization ConservativeRasterization;
    bool bDepthBounds;
    uint8 MultiViewCount;
    bool bHasFragmentDensityAttachment;
    bool bAllowVariableRateShading;
    VRSShadingRate ShadingRate;

    // Note: these flags do NOT affect compilation of this PSO.
    // The resulting object is invariant with respect to whatever is set here, they are
    // behavior hints.
    // They do not participate in equality comparisons or hashing.
    union
    {
        struct
        {
            uint16 Reserved : 14;
            uint16 bPSOPrecache : 1;
            uint16 bFromPSOFileCache : 1;
        };
        uint16 Flags;
    };

    // Cached hash off all state data provided at creation time (Only contains hash of data which influences the PSO precaching for the current platform)
    // Created from hashing the state data instead of the pointers which are used during fast runtime cache checking and compares
    uint64 StatePrecachePSOHash;
};

// 4121
enum class RenderTargetActions : uint8
{
    LoadOpMask = 2,

#define RTACTION_MAKE_MASK(Load, Store) (((uint8)RenderTargetLoadAction::Load << (uint8)LoadOpMask) | (uint8)RenderTargetStoreAction::Store)

    DontLoad_DontStore = RTACTION_MAKE_MASK(ENoAction, ENoAction),

    DontLoad_Store = RTACTION_MAKE_MASK(ENoAction, EStore),
    Clear_Store = RTACTION_MAKE_MASK(EClear, EStore),
    Load_Store = RTACTION_MAKE_MASK(ELoad, EStore),

    Clear_DontStore = RTACTION_MAKE_MASK(EClear, ENoAction),
    Load_DontStore = RTACTION_MAKE_MASK(ELoad, ENoAction),
    Clear_Resolve = RTACTION_MAKE_MASK(EClear, EMultisampleResolve),
    Load_Resolve = RTACTION_MAKE_MASK(ELoad, EMultisampleResolve),

#undef RTACTION_MAKE_MASK
};

// 4146
inline RenderTargetLoadAction GetLoadAction(RenderTargetActions Action)
{
    return (RenderTargetLoadAction)((uint8)Action >> (uint8)RenderTargetActions::LoadOpMask);
}

inline RenderTargetStoreAction GetStoreAction(RenderTargetActions Action)
{
    return (RenderTargetStoreAction)((uint8)Action & ((1 << (uint8)RenderTargetActions::LoadOpMask) - 1));
}

// 4156
enum class EDepthStencilTargetActions : uint8
{
    DepthMask = 4,

#define RTACTION_MAKE_MASK(Depth, Stencil) (((uint8)RenderTargetActions::Depth << (uint8)DepthMask) | (uint8)RenderTargetActions::Stencil)

    DontLoad_DontStore = RTACTION_MAKE_MASK(DontLoad_DontStore, DontLoad_DontStore),
    DontLoad_StoreDepthStencil = RTACTION_MAKE_MASK(DontLoad_Store, DontLoad_Store),
    DontLoad_StoreStencilNotDepth = RTACTION_MAKE_MASK(DontLoad_DontStore, DontLoad_Store),
    ClearDepthStencil_StoreDepthStencil = RTACTION_MAKE_MASK(Clear_Store, Clear_Store),
    LoadDepthStencil_StoreDepthStencil = RTACTION_MAKE_MASK(Load_Store, Load_Store),
    LoadDepthNotStencil_StoreDepthNotStencil = RTACTION_MAKE_MASK(Load_Store, DontLoad_DontStore),
    LoadDepthNotStencil_DontStore = RTACTION_MAKE_MASK(Load_DontStore, DontLoad_DontStore),
    LoadDepthStencil_StoreStencilNotDepth = RTACTION_MAKE_MASK(Load_DontStore, Load_Store),

    ClearDepthStencil_DontStoreDepthStencil = RTACTION_MAKE_MASK(Clear_DontStore, Clear_DontStore),
    LoadDepthStencil_DontStoreDepthStencil = RTACTION_MAKE_MASK(Load_DontStore, Load_DontStore),
    ClearDepthStencil_StoreDepthNotStencil = RTACTION_MAKE_MASK(Clear_Store, Clear_DontStore),
    ClearDepthStencil_StoreStencilNotDepth = RTACTION_MAKE_MASK(Clear_DontStore, Clear_Store),
    ClearDepthStencil_ResolveDepthNotStencil = RTACTION_MAKE_MASK(Clear_Resolve, Clear_DontStore),
    ClearDepthStencil_ResolveStencilNotDepth = RTACTION_MAKE_MASK(Clear_DontStore, Clear_Resolve),
    LoadDepthClearStencil_StoreDepthStencil = RTACTION_MAKE_MASK(Load_Store, Clear_Store),

    ClearStencilDontLoadDepth_StoreStencilNotDepth = RTACTION_MAKE_MASK(DontLoad_DontStore, Clear_Store),

#undef RTACTION_MAKE_MASK
};

// 4189
inline RenderTargetActions GetDepthActions(EDepthStencilTargetActions Action)
{
    return (RenderTargetActions)((uint8)Action >> (uint8)EDepthStencilTargetActions::DepthMask);
}

inline RenderTargetActions GetStencilActions(EDepthStencilTargetActions Action)
{
    return (RenderTargetActions)((uint8)Action & ((1 << (uint8)EDepthStencilTargetActions::DepthMask) - 1));
}

// 4199
struct ResolveRect
{
    int32 X1;
    int32 Y1;
    int32 X2;
    int32 Y2;

    // e.g. for a a full 256 x 256 area starting at (0, 0) it would be
    // the values would be 0, 0, 256, 256
    ResolveRect(int32 InX1 = -1, int32 InY1 = -1, int32 InX2 = -1, int32 InY2 = -1)
        : X1(InX1), Y1(InY1), X2(InX2), Y2(InY2) {}

    ResolveRect(IntVec2 Min, IntVec2 Max)
        : X1(Min.x), Y1(Min.y), X2(Max.x), Y2(Max.y) {}

    bool operator==(ResolveRect Other) const
    {
        return X1 == Other.X1 && Y1 == Other.Y1 && X2 == Other.X2 && Y2 == Other.Y2;
    }

    bool operator!=(ResolveRect Other) const { return !(*this == Other); }

    bool IsValid() const
    {
        return X1 >= 0 && Y1 >= 0 && X2 - X1 > 0 && Y2 - Y1 > 0;
    }
};

// 4238
struct RenderPassInfo
{
    struct ColorEntry
    {
        Texture *RenderTarget = nullptr;
        Texture *ResolveTarget = nullptr;
        int32 ArraySlice = -1;
        uint8 MipIndex = 0;
        RenderTargetActions Action = RenderTargetActions::DontLoad_DontStore;
    };
    std::array<ColorEntry, MaxSimultaneousRenderTargets> ColorRenderTargets;

    struct DepthStencilEntry
    {
        Texture *DepthStencilTarget = nullptr;
        Texture *ResolveTarget = nullptr;
        EDepthStencilTargetActions Action = EDepthStencilTargetActions::DontLoad_DontStore;
        ExclusiveDepthStencil ExclusiveDepthStencil;
    };
    DepthStencilEntry DepthStencilRenderTarget;

    // Controls the area for a multisample resolve or raster UAV (i.e. no fixed-function targets) operation.
    ResolveRect ResolveRect;

    // Some RHIs require a hint that occlusion queries will be used in this render pass
    uint32 NumOcclusionQueries = 0;

    // if this renderpass should be multiview, and if so how many views are required
    uint8 MultiViewCount = 0;

    // Hint for some RHI's that renderpass will have specific sub-passes
    SubpassHint SubpassHint = SubpassHint::None;

    RenderPassInfo() = default;
    RenderPassInfo(const RenderPassInfo &) = default;
    RenderPassInfo &operator=(const RenderPassInfo &) = default;

    // Color, no depth, optional resolve, optional mip, optional array slice
    explicit RenderPassInfo(Texture *ColorRT, RenderTargetActions ColorAction, Texture *ResolveRT = nullptr, uint8 InMipIndex = 0, int32 InArraySlice = -1)
    {
        check(!(ResolveRT && ResolveRT->IsMultisampled()));
        check(ColorRT);
        ColorRenderTargets[0].RenderTarget = ColorRT;
        ColorRenderTargets[0].ResolveTarget = ResolveRT;
        ColorRenderTargets[0].ArraySlice = InArraySlice;
        ColorRenderTargets[0].MipIndex = InMipIndex;
        ColorRenderTargets[0].Action = ColorAction;
    }

    inline int32 GetNumColorRenderTargets() const
    {
        int32 ColorIndex = 0;
        for (; ColorIndex < MaxSimultaneousRenderTargets; ++ColorIndex)
        {
            const ColorEntry &Entry = ColorRenderTargets[ColorIndex];
            if (!Entry.RenderTarget)
            {
                break;
            }
        }

        return ColorIndex;
    }

    void ConvertToRenderTargetsInfo(SetRenderTargetsInfo &OutRTInfo) const;
};