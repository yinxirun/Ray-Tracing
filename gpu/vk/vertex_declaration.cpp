#include "resources.h"
#include "rhi.h"
#include "gpu/core/misc/crc.h"
#include <unordered_map>
#include <memory>
#include <cstring>

struct VulkanVertexDeclarationKey
{
    VertexDeclarationElementList VertexElements;

    uint32 Hash;
    uint32 HashNoStride;

    explicit VulkanVertexDeclarationKey(const VertexDeclarationElementList &InElements)
        : VertexElements(InElements)
    {
        Hash = MemCrc_DEPRECATED(VertexElements.data(), VertexElements.size() * sizeof(VertexElement));

        HashNoStride = 0;
        for (int32 ElementIndex = 0; ElementIndex < VertexElements.size(); ElementIndex++)
        {
            VertexElement Element = VertexElements[ElementIndex];
            Element.Stride = 0;
            HashNoStride = MemCrc32(&Element, sizeof(Element), HashNoStride);
        }
    }
};
namespace std
{
    template <>
    struct hash<VulkanVertexDeclarationKey>
    {
        size_t operator()(const VulkanVertexDeclarationKey &in) const
        {
            uint64 a = in.Hash;
            uint64 b = in.HashNoStride;
            return (a << 32) | b;
        }
    };
}

bool operator==(const VulkanVertexDeclarationKey &A, const VulkanVertexDeclarationKey &B)
{
    return (A.VertexElements.size() == B.VertexElements.size() && !memcmp(A.VertexElements.data(), B.VertexElements.data(), A.VertexElements.size() * sizeof(VertexElement)));
}

VulkanVertexDeclaration::VulkanVertexDeclaration(const VertexDeclarationElementList &InElements,
                                                 uint32 InHash, uint32 InHashNoStrides)
    : Elements(InElements), Hash(InHash), HashNoStrides(InHashNoStrides) {}

std::unordered_map<VulkanVertexDeclarationKey, std::shared_ptr<VertexDeclaration>> GVertexDeclarationCache;

std::shared_ptr<VertexDeclaration> RHI::CreateVertexDeclaration(const VertexDeclarationElementList &Elements)
{
    VulkanVertexDeclarationKey Key(Elements);

    // static FCriticalSection CS;
    // FScopeLock ScopeLock(&CS);
    auto it = GVertexDeclarationCache.find(Key);
    std::shared_ptr<VertexDeclaration> VertexDeclarationRefPtr;
    if (it == GVertexDeclarationCache.end())
    {
        VertexDeclarationRefPtr = std::make_shared<VulkanVertexDeclaration>(Elements, Key.Hash, Key.HashNoStride);
        GVertexDeclarationCache.insert(std::pair(Key, VertexDeclarationRefPtr));
    }
    else
    {
        VertexDeclarationRefPtr = it->second;
    }

    check(VertexDeclarationRefPtr);

    return VertexDeclarationRefPtr;
}
