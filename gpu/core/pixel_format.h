#pragma once

#include <cstdint>
#include "gpu/definitions.h"

enum PixelFormat : uint8_t
{
    PF_Unknown = 0,
    PF_A32B32G32R32F = 1,
    PF_B8G8R8A8 = 2,
    PF_G8 = 3,  // G8  means Gray/Grey , not Green , typically actually uses a red format with replication of R to RGB
    PF_G16 = 4, // G16 means Gray/Grey like G8
    PF_DXT1 = 5,
    PF_DXT3 = 6,
    PF_DXT5 = 7,
    PF_UYVY = 8,
    PF_FloatRGB = 9,   // FloatRGB == PF_FloatR11G11B10 , NOT 16F usually, but varies
    PF_FloatRGBA = 10, // RGBA16F
    PF_DepthStencil = 11,
    PF_ShadowDepth = 12,
    PF_R32_FLOAT = 13,
    PF_D24 = 20,
    PF_X24_G8 = 44, // Used for creating SRVs to alias a DepthStencil buffer to read Stencil. Don't use for creating textures.
    PF_R32G32_UINT = 68,
    PF_R64_UINT = 84,
    PF_MAX = 92
};

enum class EPixelFormatCapabilities : uint32
{
    None = 0,
    Texture1D = 1ull << 1,
    Texture2D = 1ull << 2,
    Texture3D = 1ull << 3,
    TextureCube = 1ull << 4,
    RenderTarget = 1ull << 5,
    DepthStencil = 1ull << 6,
    TextureMipmaps = 1ull << 7,
    TextureLoad = 1ull << 8,
    TextureSample = 1ull << 9,
    TextureGather = 1ull << 10,
    TextureAtomics = 1ull << 11,
    TextureBlendable = 1ull << 12,
    TextureStore = 1ull << 13,

    Buffer = 1ull << 14,
    VertexBuffer = 1ull << 15,
    IndexBuffer = 1ull << 16,
    BufferLoad = 1ull << 17,
    BufferStore = 1ull << 18,
    BufferAtomics = 1ull << 19,

    UAV = 1ull << 20,
    TypedUAVLoad = 1ull << 21,
    TypedUAVStore = 1ull << 22,

    TextureFilterable = 1ull << 23,

    AnyTexture = Texture1D | Texture2D | Texture3D | TextureCube,

    AllTextureFlags = AnyTexture | RenderTarget | DepthStencil | TextureMipmaps | TextureLoad | TextureSample | TextureGather | TextureAtomics | TextureBlendable | TextureStore,
    AllBufferFlags = Buffer | VertexBuffer | IndexBuffer | BufferLoad | BufferStore | BufferAtomics,
    AllUAVFlags = UAV | TypedUAVLoad | TypedUAVStore,

    AllFlags = AllTextureFlags | AllBufferFlags | AllUAVFlags
};
ENUM_CLASS_FLAGS(EPixelFormatCapabilities);

/**
 * Information about a pixel format. The majority of this structure is valid after static init, however RHI does keep
 * some state in here that is initialized by that module and should not be used by general use programs that don't
 * have RHI (so noted in comments).
 */
struct PixelFormatInfo
{
    // FPixelFormatInfo() = delete;
    // FPixelFormatInfo(
    //     EPixelFormat InUnrealFormat,
    //     const char *InName,
    //     int32 InBlockSizeX,
    //     int32 InBlockSizeY,
    //     int32 InBlockSizeZ,
    //     int32 InBlockBytes,
    //     int32 InNumComponents,
    //     bool InSupported);

    const char *Name;
    PixelFormat UnrealFormat;
    int32 BlockSizeX;
    int32 BlockSizeY;
    int32 BlockSizeZ;
    int32 BlockBytes;
    int32 NumComponents;

    /** Per platform cabilities for the format (initialized by RHI module - invalid otherwise) */
    EPixelFormatCapabilities Capabilities = EPixelFormatCapabilities::None;

    /** Platform specific converted format (initialized by RHI module - invalid otherwise) */
    uint32 PlatformFormat{0};

    /** Whether the texture format is supported on the current platform/ rendering combination */
    uint8 Supported : 1;

    // If false, 32 bit float is assumed (initialized by RHI module - invalid otherwise)
    uint8 bIs24BitUnormDepthStencil : 1;
};