#pragma once
#include "gpu/definitions.h"

/** specifies an update region for a texture */
struct UpdateTextureRegion2D
{
    /** offset in texture */
    uint32 DestX;
    uint32 DestY;

    /** offset in source image data */
    int32 SrcX;
    int32 SrcY;

    /** size of region to copy */
    uint32 Width;
    uint32 Height;

    UpdateTextureRegion2D() {}

    UpdateTextureRegion2D(uint32 InDestX, uint32 InDestY, int32 InSrcX, int32 InSrcY, uint32 InWidth, uint32 InHeight)
        : DestX(InDestX), DestY(InDestY), SrcX(InSrcX), SrcY(InSrcY), Width(InWidth), Height(InHeight) {}
};