#pragma once
#include "RHIDefinitions.h"
#include "RHIAccess.h"

/** Get the best default resource state for the given texture creation flags */
extern Access RHIGetDefaultResourceState(TextureCreateFlags InUsage, bool bInHasInitialData);

/**
 * Computes the vertex count for a given number of primitives of the specified type.
 * @param NumPrimitives The number of primitives.
 * @param PrimitiveType The type of primitives.
 * @returns The number of vertices.
 */
inline uint32 GetVertexCountForPrimitiveCount(uint32 NumPrimitives, uint32 PrimitiveType)
{
    static_assert(PT_Num == 6, "This function needs to be updated");
    uint32 Factor = (PrimitiveType == PT_TriangleList) ? 3 : (PrimitiveType == PT_LineList) ? 2
                                                         : (PrimitiveType == PT_RectList)   ? 3
                                                                                            : 1;
    uint32 Offset = (PrimitiveType == PT_TriangleStrip) ? 2 : 0;
    return NumPrimitives * Factor + Offset;
}