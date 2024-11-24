#pragma once
#include "memory_archive.h"
#include "core/memory/memory_view.h"
#include <algorithm>
/**
 * Archive for reading arbitrary data from the specified memory view
 */
class MemoryReaderView : public MemoryArchive
{
public:
    virtual int64 TotalSize() override
    {
        return std::min(static_cast<int64>(Bytes.GetSize()), LimitSize);
    }

    void Serialize(void *Data, int64 Num)
    {
        if (Num && !IsError())
        {
            // Only serialize if we have the requested amount of data
            if (Offset + Num <= TotalSize())
            {
                MemoryView<void>(Data, static_cast<uint64>(Num)).CopyFrom(Bytes.Mid(Offset, Num));
                Offset += Num;
            }
            else
            {
                SetError();
            }
        }
    }

    /// @note 可能导致MemoryView中的指针失效
    explicit MemoryReaderView(std::vector<uint8> &InBytes, bool bIsPersistent = false)
        : MemoryReaderView(MakeMemoryView(InBytes), bIsPersistent)
    {
    }

    explicit MemoryReaderView(MemoryView<void> InBytes, bool bIsPersistent = false)
        : Bytes(InBytes), LimitSize(INT64_MAX)
    {
        this->SetIsLoading(true);
        this->SetIsPersistent(bIsPersistent);
    }

private:
    MemoryView<void> Bytes;
    int64 LimitSize;
};