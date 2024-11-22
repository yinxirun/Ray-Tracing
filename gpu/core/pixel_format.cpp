#include "pixel_format.h"

PixelFormatInfo::PixelFormatInfo(PixelFormat InUnrealFormat, const char *InName, int32 InBlockSizeX,
                                 int32 InBlockSizeY, int32 InBlockSizeZ, int32 InBlockBytes,
                                 int32 InNumComponents, bool InSupported)
    : Name(InName), UnrealFormat(InUnrealFormat), BlockSizeX(InBlockSizeX), BlockSizeY(InBlockSizeY), BlockSizeZ(InBlockSizeZ), BlockBytes(InBlockBytes), NumComponents(InNumComponents), Supported(InSupported), bIs24BitUnormDepthStencil(true) {}

PixelFormatInfo GPixelFormats[PF_MAX] =
    {
        // UnrealFormat Name BlockSizeX BlockSizeY BlockSizeZ BlockBytes NumComponents Supported
        PixelFormatInfo(PF_Unknown, ("unknown"), 0, 0, 0, 0, 0, 0),
        PixelFormatInfo(PF_A32B32G32R32F, ("A32B32G32R32F"), 1, 1, 1, 16, 4, 1),
        PixelFormatInfo(PF_B8G8R8A8, ("B8G8R8A8"), 1, 1, 1, 4, 4, 1),
        PixelFormatInfo(PF_G8, ("G8"), 1, 1, 1, 1, 1, 1),
        PixelFormatInfo(PF_G16, ("G16"), 1, 1, 1, 2, 1, 1),
        PixelFormatInfo(PF_DXT1, ("DXT1"), 4, 4, 1, 8, 3, 1),
        PixelFormatInfo(PF_DXT3, ("DXT3"), 4, 4, 1, 16, 4, 1),
        PixelFormatInfo(PF_DXT5, ("DXT5"), 4, 4, 1, 16, 4, 1),
        PixelFormatInfo(PF_UYVY, ("UYVY"), 2, 1, 1, 4, 4, 0),
        PixelFormatInfo(PF_FloatRGB, ("FloatRGB"), 1, 1, 1, 4, 3, 1),
        PixelFormatInfo(PF_FloatRGBA, ("FloatRGBA"), 1, 1, 1, 8, 4, 1),
        PixelFormatInfo(PF_DepthStencil, ("DepthStencil"), 1, 1, 1, 4, 1, 0),
        PixelFormatInfo(PF_ShadowDepth, ("ShadowDepth"), 1, 1, 1, 4, 1, 0),
        PixelFormatInfo(PF_R32_FLOAT, ("R32_FLOAT"), 1, 1, 1, 4, 1, 1),
        // PixelFormatInfo(PF_G16R16, ("G16R16"), 1, 1, 1, 4, 2, 1),
        // PixelFormatInfo(PF_G16R16F, ("G16R16F"), 1, 1, 1, 4, 2, 1),
        // PixelFormatInfo(PF_G16R16F_FILTER, ("G16R16F_FILTER"), 1, 1, 1, 4, 2, 1),
        // PixelFormatInfo(PF_G32R32F, ("G32R32F"), 1, 1, 1, 8, 2, 1),
        PixelFormatInfo(PF_A2B10G10R10, ("A2B10G10R10"), 1, 1, 1, 4, 4, 1),
        PixelFormatInfo(PF_A16B16G16R16, ("A16B16G16R16"), 1, 1, 1, 8, 4, 1),
        PixelFormatInfo(PF_D24, ("D24"), 1, 1, 1, 4, 1, 1),
        // PixelFormatInfo(PF_R16F, ("PF_R16F"), 1, 1, 1, 2, 1, 1),
        // PixelFormatInfo(PF_R16F_FILTER, ("PF_R16F_FILTER"), 1, 1, 1, 2, 1, 1),
        // PixelFormatInfo(PF_BC5, ("BC5"), 4, 4, 1, 16, 2, 1),
        // PixelFormatInfo(PF_V8U8, ("V8U8"), 1, 1, 1, 2, 2, 1),
        // PixelFormatInfo(PF_A1, ("A1"), 1, 1, 1, 1, 1, 0),
        // PixelFormatInfo(PF_FloatR11G11B10, ("FloatR11G11B10"), 1, 1, 1, 4, 3, 0),
        // PixelFormatInfo(PF_A8, ("A8"), 1, 1, 1, 1, 1, 1),
        // PixelFormatInfo(PF_R32_UINT, ("R32_UINT"), 1, 1, 1, 4, 1, 1),
        // PixelFormatInfo(PF_R32_SINT, ("R32_SINT"), 1, 1, 1, 4, 1, 1),

        // IOS Support
        PixelFormatInfo(PF_PVRTC2, ("PVRTC2"), 8, 4, 1, 8, 4, 0),
        PixelFormatInfo(PF_PVRTC4, ("PVRTC4"), 4, 4, 1, 8, 4, 0),

        // PixelFormatInfo(PF_R16_UINT, ("R16_UINT"), 1, 1, 1, 2, 1, 1),
        // PixelFormatInfo(PF_R16_SINT, ("R16_SINT"), 1, 1, 1, 2, 1, 1),
        // PixelFormatInfo(PF_R16G16B16A16_UINT, ("R16G16B16A16_UINT"), 1, 1, 1, 8, 4, 1),
        // PixelFormatInfo(PF_R16G16B16A16_SINT, ("R16G16B16A16_SINT"), 1, 1, 1, 8, 4, 1),
        // PixelFormatInfo(PF_R5G6B5_UNORM, ("R5G6B5_UNORM"), 1, 1, 1, 2, 3, 0),
        // PixelFormatInfo(PF_R8G8B8A8, ("R8G8B8A8"), 1, 1, 1, 4, 4, 1),
        // PixelFormatInfo(PF_A8R8G8B8, ("A8R8G8B8"), 1, 1, 1, 4, 4, 1),
        // PixelFormatInfo(PF_BC4, ("BC4"), 4, 4, 1, 8, 1, 1),
        // PixelFormatInfo(PF_R8G8, ("R8G8"), 1, 1, 1, 2, 2, 1),

        // PixelFormatInfo(PF_ATC_RGB, ("ATC_RGB"), 4, 4, 1, 8, 3, 0),
        // PixelFormatInfo(PF_ATC_RGBA_E, ("ATC_RGBA_E"), 4, 4, 1, 16, 4, 0),
        // PixelFormatInfo(PF_ATC_RGBA_I, ("ATC_RGBA_I"), 4, 4, 1, 16, 4, 0),
        // PixelFormatInfo(PF_X24_G8, ("X24_G8"), 1, 1, 1, 1, 1, 0),
        // PixelFormatInfo(PF_ETC1, ("ETC1"), 4, 4, 1, 8, 3, 0),
        // PixelFormatInfo(PF_ETC2_RGB, ("ETC2_RGB"), 4, 4, 1, 8, 3, 0),
        // PixelFormatInfo(PF_ETC2_RGBA, ("ETC2_RGBA"), 4, 4, 1, 16, 4, 0),
        // PixelFormatInfo(PF_R32G32B32A32_UINT, ("PF_R32G32B32A32_UINT"), 1, 1, 1, 16, 4, 1),
        // PixelFormatInfo(PF_R16G16_UINT, ("PF_R16G16_UINT"), 1, 1, 1, 4, 4, 1),

        // // ASTC support
        // PixelFormatInfo(PF_ASTC_4x4, ("ASTC_4x4"), 4, 4, 1, 16, 4, 0),
        // PixelFormatInfo(PF_ASTC_6x6, ("ASTC_6x6"), 6, 6, 1, 16, 4, 0),
        // PixelFormatInfo(PF_ASTC_8x8, ("ASTC_8x8"), 8, 8, 1, 16, 4, 0),
        // PixelFormatInfo(PF_ASTC_10x10, ("ASTC_10x10"), 10, 10, 1, 16, 4, 0),
        // PixelFormatInfo(PF_ASTC_12x12, ("ASTC_12x12"), 12, 12, 1, 16, 4, 0),

        // PixelFormatInfo(PF_BC6H, ("BC6H"), 4, 4, 1, 16, 3, 1),
        // PixelFormatInfo(PF_BC7, ("BC7"), 4, 4, 1, 16, 4, 1),
        // PixelFormatInfo(PF_R8_UINT, ("R8_UINT"), 1, 1, 1, 1, 1, 1),
        // PixelFormatInfo(PF_L8, ("L8"), 1, 1, 1, 1, 1, 0),
        // PixelFormatInfo(PF_XGXR8, ("XGXR8"), 1, 1, 1, 4, 4, 1),
        // PixelFormatInfo(PF_R8G8B8A8_UINT, ("R8G8B8A8_UINT"), 1, 1, 1, 4, 4, 1),
        // PixelFormatInfo(PF_R8G8B8A8_SNORM, ("R8G8B8A8_SNORM"), 1, 1, 1, 4, 4, 1),

        // PixelFormatInfo(PF_R16G16B16A16_UNORM, ("R16G16B16A16_UINT"), 1, 1, 1, 8, 4, 1),
        // PixelFormatInfo(PF_R16G16B16A16_SNORM, ("R16G16B16A16_SINT"), 1, 1, 1, 8, 4, 1),
        // PixelFormatInfo(PF_PLATFORM_HDR_0, ("PLATFORM_HDR_0"), 0, 0, 0, 0, 0, 0),
        // PixelFormatInfo(PF_PLATFORM_HDR_1, ("PLATFORM_HDR_1"), 0, 0, 0, 0, 0, 0),
        // PixelFormatInfo(PF_PLATFORM_HDR_2, ("PLATFORM_HDR_2"), 0, 0, 0, 0, 0, 0),

        // // NV12 contains 2 ures: R8 luminance plane followed by R8G8 1/4 size chrominance plane.
        // // BlockSize/BlockBytes/NumComponents values don't make much sense for this format, so set them all to one.
        // PixelFormatInfo(PF_NV12, ("NV12"), 1, 1, 1, 1, 1, 0),

        PixelFormatInfo(PF_R32G32_UINT, ("PF_R32G32_UINT"), 1, 1, 1, 8, 2, 1),

        // PixelFormatInfo(PF_ETC2_R11_EAC, ("PF_ETC2_R11_EAC"), 4, 4, 1, 8, 1, 0),
        // PixelFormatInfo(PF_ETC2_RG11_EAC, ("PF_ETC2_RG11_EAC"), 4, 4, 1, 16, 2, 0),
        // PixelFormatInfo(PF_R8, ("R8"), 1, 1, 1, 1, 1, 1),
        // PixelFormatInfo(PF_B5G5R5A1_UNORM, ("B5G5R5A1_UNORM"), 1, 1, 1, 2, 4, 0),

        // // ASTC HDR support
        // PixelFormatInfo(PF_ASTC_4x4_HDR, ("ASTC_4x4_HDR"), 4, 4, 1, 16, 4, 0),
        // PixelFormatInfo(PF_ASTC_6x6_HDR, ("ASTC_6x6_HDR"), 6, 6, 1, 16, 4, 0),
        // PixelFormatInfo(PF_ASTC_8x8_HDR, ("ASTC_8x8_HDR"), 8, 8, 1, 16, 4, 0),
        // PixelFormatInfo(PF_ASTC_10x10_HDR, ("ASTC_10x10_HDR"), 10, 10, 1, 16, 4, 0),
        // PixelFormatInfo(PF_ASTC_12x12_HDR, ("ASTC_12x12_HDR"), 12, 12, 1, 16, 4, 0),

        // PixelFormatInfo(PF_G16R16_SNORM, ("G16R16_SNORM"), 1, 1, 1, 4, 2, 1),
        // PixelFormatInfo(PF_R8G8_UINT, ("R8G8_UINT"), 1, 1, 1, 2, 2, 1),
        // PixelFormatInfo(PF_R32G32B32_UINT, ("R32G32B32_UINT"), 1, 1, 1, 12, 3, 1),
        // PixelFormatInfo(PF_R32G32B32_SINT, ("R32G32B32_SINT"), 1, 1, 1, 12, 3, 1),
        // PixelFormatInfo(PF_R32G32B32F, ("R32G32B32F"), 1, 1, 1, 12, 3, 1),
        // PixelFormatInfo(PF_R8_SINT, ("R8_SINT"), 1, 1, 1, 1, 1, 1),
        PixelFormatInfo(PF_R64_UINT, ("R64_UINT"), 1, 1, 1, 8, 1, 0),
        // PixelFormatInfo(PF_R9G9B9EXP5, ("R9G9B9EXP5"), 1, 1, 1, 4, 4, 0),

        // // P010 contains 2 ures: R16 luminance plane followed by R16G16 1/4 size chrominance plane. (upper 10 bits used)
        // // BlockSize/BlockBytes/NumComponents values don't make much sense for this format, so set them all to one.
        // PixelFormatInfo(PF_P010, ("P010"), 1, 1, 1, 2, 1, 0),

        // // ASTC high precision NormalRG support
        // PixelFormatInfo(PF_ASTC_4x4_NORM_RG, ("ASTC_4x4_NORM_RG"), 4, 4, 1, 16, 2, 0),
        // PixelFormatInfo(PF_ASTC_6x6_NORM_RG, ("ASTC_6x6_NORM_RG"), 6, 6, 1, 16, 2, 0),
        // PixelFormatInfo(PF_ASTC_8x8_NORM_RG, ("ASTC_8x8_NORM_RG"), 8, 8, 1, 16, 2, 0),
        // PixelFormatInfo(PF_ASTC_10x10_NORM_RG, ("ASTC_10x10_NORM_RG"), 10, 10, 1, 16, 2, 0),
        // PixelFormatInfo(PF_ASTC_12x12_NORM_RG, ("ASTC_12x12_NORM_RG"), 12, 12, 1, 16, 2, 0),
};
