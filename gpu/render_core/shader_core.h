#pragma once

#include "gpu/definitions.h"
#include <vector>
#include <cstring>

struct ShaderResourceTable
{
    /** Bits indicating which resource tables contain resources bound to this shader. */
    uint32 ResourceTableBits = 0;

    /** Mapping of bound SRVs to their location in resource tables. */
    std::vector<uint32> ShaderResourceViewMap;

    /** Mapping of bound sampler states to their location in resource tables. */
    std::vector<uint32> SamplerMap;

    /** Mapping of bound UAVs to their location in resource tables. */
    std::vector<uint32> UnorderedAccessViewMap;

    /** Hash of the layouts of resource tables at compile time, used for runtime validation. */
    std::vector<uint32> ResourceTableLayoutHashes;

    /** Mapping of bound Textures to their location in resource tables. */
    std::vector<uint32> TextureMap;

    friend bool operator==(const ShaderResourceTable &A, const ShaderResourceTable &B)
    {
        bool bEqual = true;
        bEqual &= (A.ResourceTableBits == B.ResourceTableBits);
        bEqual &= (A.ShaderResourceViewMap.size() == B.ShaderResourceViewMap.size());
        bEqual &= (A.SamplerMap.size() == B.SamplerMap.size());
        bEqual &= (A.UnorderedAccessViewMap.size() == B.UnorderedAccessViewMap.size());
        bEqual &= (A.ResourceTableLayoutHashes.size() == B.ResourceTableLayoutHashes.size());
        bEqual &= (A.TextureMap.size() == B.TextureMap.size());

        if (!bEqual)
        {
            return false;
        }

        bEqual &= (memcmp(A.ShaderResourceViewMap.data(), B.ShaderResourceViewMap.data(), sizeof(uint32) * A.ShaderResourceViewMap.size()) == 0);
        bEqual &= (memcmp(A.SamplerMap.data(), B.SamplerMap.data(), sizeof(uint32) * A.SamplerMap.size()) == 0);
        bEqual &= (memcmp(A.UnorderedAccessViewMap.data(), B.UnorderedAccessViewMap.data(), sizeof(uint32) * A.UnorderedAccessViewMap.size()) == 0);
        bEqual &= (memcmp(A.ResourceTableLayoutHashes.data(), B.ResourceTableLayoutHashes.data(), sizeof(uint32) * A.ResourceTableLayoutHashes.size()) == 0);
        bEqual &= (memcmp(A.TextureMap.data(), B.TextureMap.data(), sizeof(uint32) * A.TextureMap.size()) == 0);
        return bEqual;
    }
};