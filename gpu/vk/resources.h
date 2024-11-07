#pragma once

#include "Volk/volk.h"
#define VK_NO_PROTOTYPES
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vma/vk_mem_alloc.h"
#include "gpu/RHI/RHIDefinitions.h"
#include "gpu/RHI/RHIAccess.h"
#include "gpu/RHI/RHIUtilities.h"
#include "gpu/RHI/RHIResources.h"
#include "gpu/RHI/RHITransientResourceAllocator.h"
#include "gpu/core/pixel_format.h"
#include "gpu/core/assertion_macros.h"
#include "vulkan_memory.h"

#include <vector>
#include <unordered_map>

namespace VulkanRHI
{
    class StagingBuffer;
    struct PendingBufferLock;
}

class Device;
class RHICommandListBase;
class CommandListContext;
class CmdBuffer;

// Converts the internal texture dimension to Vulkan view type
inline VkImageViewType UETextureDimensionToVkImageViewType(ETextureDimension Dimension)
{
    switch (Dimension)
    {
    case ETextureDimension::Texture2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    case ETextureDimension::Texture2DArray:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case ETextureDimension::Texture3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    case ETextureDimension::TextureCube:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case ETextureDimension::TextureCubeArray:
        return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    default:
        checkNoEntry();
        return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    }
};

class View
{
public:
    struct InvalidatedState
    {
        bool bInitialized = false;
    };

    struct TypedBufferView
    {
        VkBufferView View = VK_NULL_HANDLE;
        uint32 ViewId = 0;
        bool bVolatile = false; // Whether source buffer is volatile
    };

    struct StructuredBufferView
    {
        VkBuffer Buffer = VK_NULL_HANDLE;
        uint32 HandleId = 0;
        uint32 Offset = 0;
        uint32 Size = 0;
    };

#if VULKAN_RHI_RAYTRACING
    struct AccelerationStructureView
    {
        VkAccelerationStructureKHR Handle = VK_NULL_HANDLE;
    };
#endif

    struct TextureView
    {
        VkImageView View = VK_NULL_HANDLE;
        VkImage Image = VK_NULL_HANDLE;
        uint32 ViewId = 0;
    };

    enum EType
    {
        Null,
        TypedBuffer,
        Texture,
        StructuredBuffer,
#if VULKAN_RHI_RAYTRACING
        AccelerationStructure,
#endif
    };

    View(Device &InDevice, VkDescriptorType InDescriptorType);

    ~View();

    void Invalidate();

    EType GetViewType() const { return viewType; }

    bool IsInitialized() const { return (GetViewType() != Null) || invalidatedState.bInitialized; }

    TextureView const &GetTextureView() const { return textureView; }

    View *InitAsTextureView(VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, PixelFormat UEFormat,
                            VkFormat Format, uint32_t FirstMip, uint32_t NumMips, uint32_t ArraySliceIndex, uint32_t NumArraySlices, bool bUseIdentitySwizzle = false,
                            VkImageUsageFlags ImageUsageFlags = 0);

private:
    Device &device;
    RHIDescriptorHandle BindlessHandle;

    EType viewType = EType::Null;
    InvalidatedState invalidatedState;
    TypedBufferView typedBufferView;
    StructuredBufferView structuredBufferView;
#if VULKAN_RHI_RAYTRACING
    AccelerationStructureView accelerationStructureView;
#endif
    TextureView textureView;
};

// 64
/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class VulkanVertexDeclaration : public VertexDeclaration
{
public:
    VertexDeclarationElementList Elements;
    uint32 Hash;
    uint32 HashNoStrides;

    VulkanVertexDeclaration(const VertexDeclarationElementList &InElements, uint32 InHash, uint32 InHashNoStrides);

    virtual uint32 GetPrecachePSOHash() const final override { return HashNoStrides; }
};

// 87
class VulkanShaderModule //: public FThreadSafeRefCountedObject
{
    static Device *device;
    VkShaderModule ActualShaderModule;

public:
    VulkanShaderModule(Device *DeviceIn, VkShaderModule ShaderModuleIn) : ActualShaderModule(ShaderModuleIn)
    {
        check(DeviceIn && (device == DeviceIn || !device));
        device = DeviceIn;
    }
    virtual ~VulkanShaderModule();
    VkShaderModule &GetVkShaderModule() { return ActualShaderModule; }
};

// 101
class VulkanShader //: public IRefCountedObject
{
public:
    VulkanShader(Device *InDevice, EShaderFrequency InFrequency)
        : ShaderKey(0), Frequency(InFrequency), device(InDevice) {}

    virtual ~VulkanShader();

    inline uint64 GetShaderKey() const { return ShaderKey; }

protected:
    uint64 ShaderKey;
    std::vector<uint8> spirv;
    const EShaderFrequency Frequency;

protected:
    void Setup(std::vector<uint8> &&spirv, uint64 shaderKey);
    Device *device;

    friend class CommandListContext;
    friend class VulkanShaderFactory;
};

/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
template <typename BaseResourceType, EShaderFrequency ShaderType>
class TVulkanBaseShader : public BaseResourceType, public VulkanShader
{
private:
    TVulkanBaseShader(Device *InDevice) : VulkanShader(InDevice, ShaderType) {}
    friend class VulkanShaderFactory;

public:
    enum
    {
        StaticFrequency = ShaderType
    };
};
typedef TVulkanBaseShader<VertexShader, SF_Vertex> VulkanVertexShader;
typedef TVulkanBaseShader<PixelShader, SF_Pixel> VulkanPixelShader;
typedef TVulkanBaseShader<RHIComputeShader, SF_Compute> VulkanComputeShader;

// 315
class VulkanShaderFactory
{
public:
    template <typename ShaderType>
    ShaderType *CreateShader(std::vector<uint8>& Code, Device *Device);
};

class VulkanBoundShaderState : public BoundShaderState
{
};

struct CpuReadbackBuffer
{
    VkBuffer Buffer;
    uint32 MipOffsets[MAX_TEXTURE_MIP_COUNT];
    uint32 MipSize[MAX_TEXTURE_MIP_COUNT];
};

// 576
class LinkedView : public View
{
protected:
    LinkedView(Device &Device, VkDescriptorType DescriptorType)
        : View(Device, DescriptorType) {}
};

// 542
class VulkanViewableResource
{
public:
    // @todo convert views owned by the texture into proper
    // FVulkanView instances, then remove 'virtual' from this class
    virtual void UpdateLinkedViews();

private:
    LinkedView *LinkedViews = nullptr;
};

enum class EImageOwnerType : uint8
{
    None,
    LocalOwner,
    ExternalOwner,
    Aliased
};

class VulkanTexture : public Texture
{
public:
    // Regular constructor.
    VulkanTexture(RHICommandListBase *RHICmdList, Device &InDevice, const TextureCreateDesc &InCreateDesc, bool bIsTransientResource = false);

    VulkanTexture(Device &InDevice, const TextureCreateDesc &InCreateDesc)
        : VulkanTexture(nullptr, InDevice, InCreateDesc)
    {
    }

    // Construct from external resource.
    // FIXME: HUGE HACK: the bUnused argument is there to disambiguate this overload from the one above when passing nullptr, since nullptr is a valid VkImage. Get rid of this code smell when unifying FVulkanSurface and FVulkanTexture.
    VulkanTexture(Device &InDevice, const TextureCreateDesc &InCreateDesc, VkImage InImage, bool bUnused);

    virtual ~VulkanTexture();

    /// View with all mips/layers
    View *DefaultView = nullptr;
    // View with all mips/layers, but if it's a Depth/Stencil, only the Depth view
    View *PartialView = nullptr;

    void *GetTextureBaseRHI() override final { return this; }

    struct ImageCreateInfo
    {
        VkImageCreateInfo ImageCreateInfo;
        // only used when HasImageFormatListKHR is supported. Otherise VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT is used.
        VkImageFormatListCreateInfoKHR ImageFormatListCreateInfo;
        // used when TexCreate_External is given
        VkExternalMemoryImageCreateInfoKHR ExternalMemImageCreateInfo;
        // Array of formats used for mutable formats
        std::vector<VkFormat> FormatsUsed;
    };

    // Full includes Depth+Stencil
    inline VkImageAspectFlags GetFullAspectMask() const { return FullAspectMask; }

    inline bool IsImageOwner() const { return (ImageOwnerType == EImageOwnerType::LocalOwner); }

    // Seperate method for creating VkImageCreateInfo
    static void GenerateImageCreateInfo(
        ImageCreateInfo &OutImageCreateInfo,
        Device &InDevice,
        const TextureDesc &InDesc,
        VkFormat *OutStorageFormat = nullptr,
        VkFormat *OutViewFormat = nullptr,
        bool bForceLinearTexture = false);

    void DestroySurface();

    inline VkImageViewType GetViewType() const
    {
        return UETextureDimensionToVkImageViewType(GetDesc().Dimension);
    }

    inline uint32 GetNumberOfArrayLevels() const
    {
        switch (GetViewType())
        {
        case VK_IMAGE_VIEW_TYPE_1D:
        case VK_IMAGE_VIEW_TYPE_2D:
        case VK_IMAGE_VIEW_TYPE_3D:
            return 1;
        case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
            return GetDesc().ArraySize;
        case VK_IMAGE_VIEW_TYPE_CUBE:
            return 6;
        case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
            return 6 * GetDesc().ArraySize;
        default:
            assert(!"dlksjflkdjslkaf");
            // ErrorInvalidViewType();
            return 1;
        }
    }

    inline bool IsDepthOrStencilAspect() const
    {
        return (FullAspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
    }

    inline bool SupportsSampling() const
    {
        return true;
        // return EnumHasAllFlags(GPixelFormats[GetDesc().Format].Capabilities, EPixelFormatCapabilities::TextureSample);
    }

    inline VkImageLayout GetDefaultLayout() const { return DefaultLayout; }

    static void InternalLockWrite(CommandListContext &Context, VulkanTexture *Surface,
                                  const VkBufferImageCopy &Region, VulkanRHI::StagingBuffer *StagingBuffer);

    Device *device;
    VkImage Image;
    VkImageUsageFlags ImageUsageFlags;
    VkFormat StorageFormat; // Removes SRGB if requested, used to upload data
    VkFormat ViewFormat;    // Format for SRVs, render targets
    VkMemoryPropertyFlags MemProps;
    VkMemoryRequirements MemoryRequirements;

private:
    void SetInitialImageState(CommandListContext &Context, VkImageLayout InitialLayout,
                              bool bClear, const ClearValueBinding &ClearValueBinding, bool bIsTransientResource);
    VkImageTiling Tiling;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo{};
    VkImageAspectFlags FullAspectMask{};
    VkImageAspectFlags PartialAspectMask{};
    CpuReadbackBuffer *cpuReadbackBuffer = nullptr;
    VkImageLayout DefaultLayout{};

protected:
    EImageOwnerType ImageOwnerType;
};

// 1401
class VulkanGPUFence : public GPUFence
{
public:
    VulkanGPUFence() {}
    virtual void Clear() final override;
    virtual bool Poll() const final override;

    CmdBuffer *GetCmdBuffer() const { return CmdBuffer; }

protected:
    CmdBuffer *CmdBuffer = nullptr;
    uint64 FenceSignaledCounter = UINT64_MAX;

    friend class CommandListContext;
};

// 1068
class VulkanMultiBuffer : public Buffer, public VulkanRHI::DeviceChild, public VulkanViewableResource
{
public:
    VulkanMultiBuffer(Device *InDevice, BufferDesc const &InBufferDesc,
                      ResourceCreateInfo &CreateInfo, class RHICommandListBase *InRHICmdList = nullptr,
                      const RHITransientHeapAllocation *InTransientHeapAllocation = nullptr);
    virtual ~VulkanMultiBuffer();

    inline VkBuffer GetHandle() const { return BufferAllocs[CurrentBufferIndex].handle; }
    inline uint32 GetOffset() const { return BufferAllocs[CurrentBufferIndex].offset; }
    inline VkIndexType GetIndexType() const { return (GetStride() == 4) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16; }
    void *Lock(RHICommandListBase &RHICmdList, ResourceLockMode LockMode, uint32 Size, uint32 Offset);
    void *Lock(CommandListContext &Context, ResourceLockMode LockMode, uint32 Size, uint32 Offset);
    inline void Unlock(RHICommandListBase &RHICmdList) { Unlock(&RHICmdList, nullptr); }
    inline void Unlock(CommandListContext &Context) { Unlock(nullptr, &Context); }

    void ReleaseOwnership();

    static VkBufferUsageFlags UEToVKBufferUsageFlags(Device *InDevice, BufferUsageFlags InUEUsage, bool bZeroSize);

protected:
    void AdvanceBufferIndex();
    void UpdateBufferAllocStates(CommandListContext &Context);

    void Unlock(RHICommandListBase *RHICmdList, CommandListContext *Context);

    VkBufferUsageFlags usageFlags;

    enum class LockStatus : uint8
    {
        Unlocked,
        Locked,
        PersistentMapping,
    } lockStatus = LockStatus::Unlocked;

    struct BufferAlloc
    {
        VkBuffer handle;
        VkDeviceSize offset = 0;
        VmaAllocation alloc = VK_NULL_HANDLE;
        VmaAllocationInfo allocInfo{};
        void *HostPtr = nullptr;
        class VulkanGPUFence *Fence = nullptr;
        VkDeviceAddress DeviceAddress = 0;

        enum class AllocStatus : uint8
        {
            Available,  // The allocation is ready to be used
            InUse,      // CurrentBufferIndex should point to this allocation
            NeedsFence, // The allocation was just released and needs a fence to make sure previous commands are done with it
            Pending,    // Fence was written, we are waiting on it to know that the alloc can be used again
        } allocStatus = AllocStatus::Available;
    };
    std::vector<BufferAlloc> BufferAllocs;
    int32 CurrentBufferIndex = -1;
    uint32 LockCounter = 0;

    static void InternalUnlock(CommandListContext &Context, VulkanRHI::PendingBufferLock &PendingLock,
                               VulkanMultiBuffer *MultiBuffer, int32 InDynamicBufferIndex);
};

// 1258
class VertexInputStateInfo
{
public:
    // 	FVulkanVertexInputStateInfo();
    // 	~FVulkanVertexInputStateInfo();

    // 	void Generate(FVulkanVertexDeclaration* VertexDeclaration, uint32 VertexHeaderInOutAttributeMask);

    // 	inline uint32 GetHash() const
    // 	{
    // 		check(Info.sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
    // 		return Hash;
    // 	}

    // 	inline const VkPipelineVertexInputStateCreateInfo& GetInfo() const
    // 	{
    // 		return Info;
    // 	}

    // 	bool operator ==(const FVulkanVertexInputStateInfo& Other);

protected:
    // VkPipelineVertexInputStateCreateInfo Info;
    // uint32 Hash;

    uint32 BindingsNum;
    uint32 BindingsMask;

    // #todo-rco: Remove these TMaps
    std::unordered_map<uint32, uint32> BindingToStream;
    std::unordered_map<uint32, uint32> StreamToBinding;
    VkVertexInputBindingDescription Bindings[MaxVertexElementCount];

    uint32 AttributesNum;
    VkVertexInputAttributeDescription Attributes[MaxVertexElementCount];

    friend class PendingGfxState;
    // 	friend class FVulkanPipelineStateCacheManager;
};

// 1278
//  Layout for a Pipeline, also includes DescriptorSets layout
class VulkanLayout : public VulkanRHI::DeviceChild
{
    // public:
    // 	FVulkanLayout(FVulkanDevice* InDevice);
    // 	virtual ~FVulkanLayout();

    // 	virtual bool IsGfxLayout() const = 0;

    // 	inline const FVulkanDescriptorSetsLayout& GetDescriptorSetsLayout() const
    // 	{
    // 		return DescriptorSetLayout;
    // 	}

    // 	inline VkPipelineLayout GetPipelineLayout() const
    // 	{
    // 		return Device->SupportsBindless() ? Device->GetBindlessDescriptorManager()->GetPipelineLayout() : PipelineLayout;
    // 	}

    // 	inline bool HasDescriptors() const
    // 	{
    // 		return DescriptorSetLayout.GetLayouts().Num() > 0;
    // 	}

    // 	inline uint32 GetDescriptorSetLayoutHash() const
    // 	{
    // 		return DescriptorSetLayout.GetHash();
    // 	}

    // 	void PatchSpirvBindings(FVulkanShader::FSpirvCode& SpirvCode, EShaderFrequency Frequency, const FVulkanShaderHeader& CodeHeader) const;

    // protected:
    // 	FVulkanDescriptorSetsLayout	DescriptorSetLayout;
    // 	VkPipelineLayout			PipelineLayout;

    // 	template <bool bIsCompute>
    // 	inline void FinalizeBindings(const FUniformBufferGatherInfo& UBGatherInfo)
    // 	{
    // 		// Setting descriptor is only allowed prior to compiling the layout
    // 		check(DescriptorSetLayout.GetHandles().Num() == 0);

    // 		DescriptorSetLayout.FinalizeBindings<bIsCompute>(UBGatherInfo);
    // 	}

    // 	inline void ProcessBindingsForStage(VkShaderStageFlagBits StageFlags, ShaderStage::EStage DescSet, const FVulkanShaderHeader& CodeHeader, FUniformBufferGatherInfo& OutUBGatherInfo) const
    // 	{
    // 		// Setting descriptor is only allowed prior to compiling the layout
    // 		check(DescriptorSetLayout.GetHandles().Num() == 0);

    // 		DescriptorSetLayout.ProcessBindingsForStage(StageFlags, DescSet, CodeHeader, OutUBGatherInfo);
    // 	}

    // 	void Compile(FVulkanDescriptorSetLayoutMap& DSetLayoutMap);

    // 	friend class FVulkanComputePipeline;
    // 	friend class FVulkanGfxPipeline;
    // 	friend class FVulkanPipelineStateCacheManager;
    // #if VULKAN_RHI_RAYTRACING
    // 	friend class FVulkanRayTracingPipelineState;
    // #endif
};
class VulkanGfxLayout : public VulkanLayout
{
    // public:
    // 	FVulkanGfxLayout(FVulkanDevice* InDevice)
    // 		: FVulkanLayout(InDevice)
    // 	{
    // 	}

    // 	virtual bool IsGfxLayout() const final override
    // 	{
    // 		return true;
    // 	}

    // 	inline const FVulkanGfxPipelineDescriptorInfo& GetGfxPipelineDescriptorInfo() const
    // 	{
    // 		return GfxPipelineDescriptorInfo;
    // 	}

    // 	bool UsesInputAttachment(FVulkanShaderHeader::EAttachmentType AttachmentType) const;

    // protected:
    // 	FVulkanGfxPipelineDescriptorInfo		GfxPipelineDescriptorInfo;
    // 	friend class FVulkanPipelineStateCacheManager;
};

static VulkanTexture *ResourceCast(Texture *texture)
{
    return static_cast<VulkanTexture *>(texture->GetTextureBaseRHI());
}