#include "resources.h"
#include "rhi.h"
#include "gpu/core/misc/crc.h"
#include "private.h"
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

VertexInputStateInfo::VertexInputStateInfo()
    : Hash(0), BindingsNum(0), BindingsMask(0), AttributesNum(0)
{
    memset(&Info, 0, sizeof(VkPipelineVertexInputStateCreateInfo));
    memset(Attributes.data(), 0, sizeof(VkVertexInputAttributeDescription) * Attributes.size());
    memset(Bindings.data(), 0, sizeof(VkVertexInputBindingDescription) * Bindings.size());
}

bool VertexInputStateInfo::operator==(const VertexInputStateInfo &Other)
{
    if (AttributesNum != Other.AttributesNum)
    {
        return false;
    }
    for (uint32 i = 0; i < AttributesNum; ++i)
    {
        if (0 != memcmp(&Attributes[i], &Other.Attributes[i], sizeof(Attributes[i])))
        {
            return false;
        }
    }
    return true;
}

/// 104 创建PSO需要VkPipelineVertexInputStateCreateInfo，其由VkVertexInputBindingDescription和VkVertexInputAttributeDescription两部分信息组成。
/// 本函数根据输入VertexDeclaration生成以上信息，本保存在本对象内。同时生成哈希值。
void VertexInputStateInfo::Generate(VulkanVertexDeclaration *VertexDeclaration, uint32 VertexHeaderInOutAttributeMask)
{
    // GenerateVertexInputState is expected to be called only once!
    check(Info.sType == 0);

    // Generate vertex attribute Layout
    const VertexDeclarationElementList &VertexInput = VertexDeclaration->Elements;

    // Generate Bindings
    for (const VertexElement &Element : VertexInput)
    {
        if ((1 << Element.AttributeIndex) & VertexHeaderInOutAttributeMask)
        {
            check(Element.StreamIndex < MaxVertexElementCount);

            VkVertexInputBindingDescription &CurrBinding = Bindings[Element.StreamIndex];
            if ((BindingsMask & (1 << Element.StreamIndex)) != 0)
            {
                // If exists, validate.
                // Info must be the same
                check(CurrBinding.binding == Element.StreamIndex);
                check(CurrBinding.inputRate == Element.bUseInstanceIndex ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX);
                check(CurrBinding.stride == Element.Stride);
            }
            else
            {
                // Zeroed outside
                check(CurrBinding.binding == 0 && CurrBinding.inputRate == 0 && CurrBinding.stride == 0);
                CurrBinding.binding = Element.StreamIndex;
                CurrBinding.inputRate = Element.bUseInstanceIndex ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
                CurrBinding.stride = Element.Stride;

                // Add mask flag and increment number of bindings
                BindingsMask |= 1 << Element.StreamIndex;
            }
        }
    }

    // Remove gaps between bindings
    BindingsNum = 0;
    BindingToStream.clear();
    StreamToBinding.clear();
    for (int32 i = 0; i < Bindings.size(); i++)
    {
        if (!((1 << i) & BindingsMask))
        {
            continue;
        }

        BindingToStream.insert(std::pair(BindingsNum, i));
        StreamToBinding.insert(std::pair(i, BindingsNum));
        VkVertexInputBindingDescription &CurrBinding = Bindings[BindingsNum];
        CurrBinding = Bindings[i];
        CurrBinding.binding = BindingsNum;
        BindingsNum++;
    }

    // Clean originally placed bindings
    memset(Bindings.data() + BindingsNum, 0, sizeof(Bindings[0]) * (Bindings.size() - BindingsNum));

    // Attributes are expected to be uninitialized/empty
    check(AttributesNum == 0);
    for (const VertexElement &CurrElement : VertexInput)
    {
        // Mask-out unused vertex input
        if ((!((1 << CurrElement.AttributeIndex) & VertexHeaderInOutAttributeMask)) || StreamToBinding.count(CurrElement.StreamIndex) == 0)
        {
            continue;
        }

        VkVertexInputAttributeDescription &CurrAttribute = Attributes[AttributesNum++]; // Zeroed at the begin of the function
        check(CurrAttribute.location == 0 && CurrAttribute.binding == 0 && CurrAttribute.format == 0 && CurrAttribute.offset == 0);

        CurrAttribute.binding = StreamToBinding[CurrElement.StreamIndex];
        CurrAttribute.location = CurrElement.AttributeIndex;
        CurrAttribute.format = UEToVkBufferFormat(CurrElement.Type);
        CurrAttribute.offset = CurrElement.Offset;
    }

    // Vertex Input
    Info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    // Its possible to have no vertex buffers
    if (BindingsNum == 0)
    {
        check(Hash == 0);
        return;
    }

    Info.vertexBindingDescriptionCount = BindingsNum;
    Info.pVertexBindingDescriptions = Bindings.data();

    check(AttributesNum > 0);
    Info.vertexAttributeDescriptionCount = AttributesNum;
    Info.pVertexAttributeDescriptions = Attributes.data();

    Hash = MemCrc32(Bindings.data(), BindingsNum * sizeof(Bindings[0]));
    check(AttributesNum > 0);
    Hash = MemCrc32(Attributes.data(), AttributesNum * sizeof(Attributes[0]), Hash);
}
