#pragma once
#include "Volk/volk.h"
#include "gpu/definitions.h"
#include "RHIDefinitions.h"
#include "RHIAccess.h"
#include "RHIUtilities.h"

/** Descriptor used to create a texture resource */
struct RHITextureDesc
{
    RHITextureDesc() = default;
    RHITextureDesc(ETextureDimension InDimension) : Dimension(InDimension) {}

    /** Texture dimension to use when creating the RHI texture. */
    ETextureDimension Dimension = ETextureDimension::Texture2D;

    /** Pixel format used to create RHI texture. */
    VkFormat Format = VK_FORMAT_UNDEFINED;

    /** Extent of the texture in x and y. */
    VkExtent2D Extent = {1, 1};

    /** Depth of the texture if the dimension is 3D. */
    uint16 Depth = 1;

    /** The number of array elements in the texture. (Keep at 1 if dimension is 3D). */
    uint16 ArraySize = 1;

    /** Number of mips in the texture mip-map chain. */
    uint8 NumMips = 1;

    /** Texture flags passed on to RHI texture. */
    ETextureCreateFlags Flags = TexCreate_None;
};

struct RHITextureCreateDesc : public RHITextureDesc
{
    static RHITextureCreateDesc Create2D(const char *InDebugName)
    {
        return RHITextureCreateDesc(InDebugName, ETextureDimension::Texture2D);
    }

    static RHITextureCreateDesc Create2D(const char *DebugName, int32 SizeX, int32 SizeY, VkFormat Format)
    {
        return Create2D(DebugName).SetExtent(SizeX, SizeY).SetFormat(Format);
    }

    // Constructor with minimal argument set. Name and dimension are always required.
    RHITextureCreateDesc(const char *InDebugName, ETextureDimension InDimension) : RHITextureDesc(InDimension), DebugName(InDebugName) {}

    RHITextureCreateDesc &SetExtent(int32 InExtentX, int32 InExtentY)
    {
        Extent.width = InExtentX;
        Extent.height = InExtentY;
        return *this;
    }
    RHITextureCreateDesc &SetFormat(VkFormat InFormat)
    {
        Format = InFormat;
        return *this;
    }
    RHITextureCreateDesc &SetFlags(ETextureCreateFlags InFlags)
    {
        Flags = InFlags;
        return *this;
    }
    RHITextureCreateDesc &DetermineInititialState()
    {
        if (InitialState == ERHIAccess::Unknown)
            InitialState = RHIGetDefaultResourceState(Flags, BulkData != nullptr);
        return *this;
    }

    /* The RHI access state that the resource will be created in. */
    ERHIAccess InitialState = ERHIAccess::Unknown;

    /* A friendly name for the resource. */
    const char *DebugName = nullptr;

    void *BulkData = nullptr;
};

class RHIResource
{
public:
    RHIResource() = default;
};

class RHIViewableResource : public RHIResource
{
public:
    RHIViewableResource(ERHIResourceType InResourceType, ERHIAccess InAccess);
};

class RHITexture : public RHIViewableResource
{
public:
    RHITexture(const RHITextureCreateDesc &InDesc);

    /**
     * Get the texture description used to create the texture
     * Still virtual because FRHITextureReference can override this function - remove virtual when FRHITextureReference is deprecated
     *
     * @return TextureDesc used to create the texture
     */
    virtual const RHITextureDesc &GetDesc() const { return TextureDesc; }

private:
    RHITextureDesc TextureDesc;
};