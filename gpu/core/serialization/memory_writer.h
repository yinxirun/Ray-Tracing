#include "memory_archive.h"
#include <type_traits>
#include <vector>
#include <iostream>

template <int IndexSize>
struct TBitsToSizeType
{
    static_assert(IndexSize, "Unsupported allocator index size.");
};

template <>
struct TBitsToSizeType<8>
{
    using Type = int8;
};
template <>
struct TBitsToSizeType<16>
{
    using Type = int16;
};
template <>
struct TBitsToSizeType<32>
{
    using Type = int32;
};
template <>
struct TBitsToSizeType<64>
{
    using Type = int64;
};

/**
 * Archive for storing arbitrary data to the specified memory location
 */
template <int IndexSize>
class TMemoryWriter : public MemoryArchive
{
    static_assert(IndexSize == 32 || IndexSize == 64, "Only 32-bit and 64-bit index sizes supported");
    using IndexSizeType = typename TBitsToSizeType<IndexSize>::Type;

public:
    TMemoryWriter(std::vector<uint8> &InBytes, bool bIsPersistent = false, bool bSetOffset = false)
        : MemoryArchive(), Bytes(InBytes)
    {
        this->SetIsSaving(true);
        this->SetIsPersistent(bIsPersistent);
        if (bSetOffset)
        {
            Offset = InBytes.size();
        }
    }

    virtual void Serialize(void *Data, int64 Num) override
    {
        const int64 NumBytesToAdd = Offset + Num - Bytes.size();
        if (NumBytesToAdd > 0)
        {
            const int64 NewArrayCount = Bytes.size() + NumBytesToAdd;

            if constexpr (IndexSize == 32)
            {
                if (NewArrayCount >= INT32_MAX)
                {
                    printf("MemoryWriter does not support data larger than 2GB. %s %d\n", __FILE__, __LINE__);
                    exit(-1);
                }
            }

            Bytes.resize((IndexSizeType)NumBytesToAdd);
        }

        check((Offset + Num) <= Bytes.size());

        if (Num)
        {
            memcpy(&Bytes[(IndexSizeType)Offset], Data, Num);
            Offset += Num;
        }
    }

    int64 TotalSize() override { return Bytes.size(); }

protected:
    std::vector<uint8> &Bytes;
};

// FMemoryWriter and FMemoryWriter64 are implemented as derived classes rather than aliases
// so that forward declarations will work.

class MemoryWriter : public TMemoryWriter<32>
{
    using Super = TMemoryWriter<32>;

public:
    using Super::Super;
};

class MemoryWriter64 : public TMemoryWriter<64>
{
    using Super = TMemoryWriter<64>;

public:
    using Super::Super;
};