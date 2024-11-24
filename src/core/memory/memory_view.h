#pragma once

#include <type_traits>
#include <algorithm>
#include "definitions.h"
#include "core/templates/choose_class.h"
#include "core/assertion_macros.h"

/**
 * A non-owning view of a contiguous region of memory.
 *
 * Prefer to use the aliases FMemoryView or FMutableMemoryView over this type.
 *
 * Functions that modify a view clamp sizes and offsets to always return a sub-view of the input.
 */
template <typename DataType>
class MemoryView
{
    static_assert(std::is_void_v<DataType>, "DataType must be cv-qualified void");

    using ByteType = typename TChooseClass<std::is_const<DataType>::value, const uint8, uint8>::Result;

public:
    /** Construct an empty view. */
    constexpr MemoryView() = default;

    /** Construct a view of by copying a view with compatible const/volatile qualifiers. */
    constexpr inline MemoryView(const MemoryView<DataType> &InView) : Data(InView.Data), Size(InView.Size) {}

    /** Construct a view of InSize bytes starting at InData. */
    constexpr inline MemoryView(DataType *InData, uint64 InSize) : Data(InData), Size(InSize) {}

    /** Returns the number of bytes in the view. */
    constexpr inline uint64 GetSize() const { return Size; }

    /** Returns the right-most part of the view by chopping the given number of bytes from the left. */
    inline MemoryView RightChop(uint64 InSize) const
    {
        MemoryView View(*this);
        View.RightChopInline(InSize);
        return View;
    }

    /** Copies bytes from the input view into this view, and returns the remainder of this view. */
    inline MemoryView CopyFrom(MemoryView<void> InView) const
    {
        check(InView.Size <= Size);
        if (InView.Size)
        {
            memcpy(Data, InView.Data, InView.Size);
        }
        return RightChop(InView.Size);
    }

    /** Returns the middle part of the view by taking up to the given number of bytes from the given position. */
    inline MemoryView Mid(uint64 InOffset, uint64 InSize) const
    {
        MemoryView View(*this);
        View.MidInline(InOffset, InSize);
        return View;
    }

    /** Modifies the view to be the given number of bytes from the left. */
    constexpr inline void LeftInline(uint64 InSize)
    {
        Size = std::min(Size, InSize);
    }

    /** Modifies the view by chopping the given number of bytes from the left. */
    inline void RightChopInline(uint64 InSize)
    {
        const uint64 Offset = std::min(Size, InSize);
        Data = GetDataAtOffsetNoCheck(Offset);
        Size -= Offset;
    }

    /** Modifies the view to be the middle part by taking up to the given number of bytes from the given offset. */
    inline void MidInline(uint64 InOffset, uint64 InSize)
    {
        RightChopInline(InOffset);
        LeftInline(InSize);
    }

private:
    /** Returns the data pointer advanced by an offset in bytes. */
    inline DataType *GetDataAtOffsetNoCheck(uint64 InOffset) const
    {
        return reinterpret_cast<ByteType *>(Data) + InOffset;
    }

    DataType *Data = nullptr;
    uint64 Size = 0;
};

/** Make a non-owning view of the memory of the contiguous container. */
template <typename ContainerType>
constexpr inline auto MakeMemoryView(ContainerType &&Container)
{
    using ElementType = typename std::remove_pointer<decltype((std::declval<ContainerType>()).data())>::type;
    constexpr bool bIsConst = std::is_const<ElementType>::value;
    using DataType = typename TChooseClass<bIsConst, const void, void>::Result;
    return MemoryView<DataType>((Container).data(), (Container).size() * sizeof(ElementType));
}