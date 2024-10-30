#pragma once

#include "Volk/volk.h"
#include "gpu/RHI/RHIDefinitions.h"
#include "gpu/RHI/RHIAccess.h"
#include "gpu/RHI/RHIUtilities.h"
#include "gpu/RHI/RHIResources.h"
#include "gpu/core/pixel_format.h"
#include "gpu/core/assertion_macros.h"

class Device;

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
}

class Resource
{
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

    View *InitAsTextureView(VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat,
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

enum class EImageOwnerType : uint8
{
    None,
    LocalOwner,
    ExternalOwner,
    Aliased
};

class Texture : public RHITexture
{
public:
    // Construct from external resource.
    // FIXME: HUGE HACK: the bUnused argument is there to disambiguate this overload from the one above when passing nullptr, since nullptr is a valid VkImage. Get rid of this code smell when unifying FVulkanSurface and FVulkanTexture.
    Texture(Device &InDevice, const RHITextureCreateDesc &InCreateDesc, VkImage InImage, bool bUnused);

    virtual ~Texture();

    // View with all mips/layers
    View *DefaultView = nullptr;
    // View with all mips/layers, but if it's a Depth/Stencil, only the Depth view
    View *PartialView = nullptr;

    // Full includes Depth+Stencil
    inline VkImageAspectFlags GetFullAspectMask() const { return FullAspectMask; }

    inline bool IsImageOwner() const { return (ImageOwnerType == EImageOwnerType::LocalOwner); }

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

    Device *device;
    VkImage Image;
    VkImageUsageFlags ImageUsageFlags;
    VkMemoryPropertyFlags MemProps;
    VkMemoryRequirements MemoryRequirements;

private:
    VkImageAspectFlags FullAspectMask;

protected:
    EImageOwnerType ImageOwnerType;
};