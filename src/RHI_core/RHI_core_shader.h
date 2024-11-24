#pragma once

#include "RHI/RHIResources.h"
#include "render_core/shader_core.h"
namespace RHICore
{
    static __forceinline uint32 CountTrailingZeros(uint32 value)
    {
        uint32 temp = 1;
        for (int i = 0; i < 32; ++i)
        {
            if (temp & value)
                return i;
            temp = temp << 1;
        }
        return 32;
    }

    template <typename TBinder, typename TUniformBufferArrayType, typename TBitMaskType>
    void SetResourcesFromTables(TBinder &&Binder, RHIShader const &Shader,
                                ShaderResourceTable const &SRT, TBitMaskType &DirtyUniformBuffers,
                                TUniformBufferArrayType const &BoundUniformBuffers)
    {
        // Mask the dirty bits by those buffers from which the shader has bound resources.
        uint32 DirtyBits = SRT.ResourceTableBits & DirtyUniformBuffers;

        while (DirtyBits)
        {
            // Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
            const uint32 LowestBitMask = (DirtyBits) & (-(int32)DirtyBits);

            const int32 BufferIndex = CountTrailingZeros(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
            DirtyBits ^= LowestBitMask;

            check(BufferIndex < SRT.ResourceTableLayoutHashes.size());

            UniformBuffer *Buffer = BoundUniformBuffers[BufferIndex];
        }

        DirtyUniformBuffers = TBitMaskType(0);
    }
}