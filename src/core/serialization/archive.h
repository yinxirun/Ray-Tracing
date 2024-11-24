#pragma once
#include "definitions.h"
#include "core/containers/enum_as_byte.h"
#include <iostream>

#define PLATFORM_LITTLE_ENDIAN 1

struct ArchiveState
{
private:
    // Only FArchive is allowed to instantiate this, by inheritance
    friend class Archive;

public:
    /** Returns lowest level archive state, proxy archives will override this. */
    virtual ArchiveState &GetInnermostState() { return *this; }

    /** Returns total size of the backing data storage. */
    virtual int64 TotalSize() { return -1; }

    /** Returns true if this archive contains errors, which means that further serialization is generally not safe. */
    __forceinline bool IsError() const { return ArIsError; }

    /** Returns true if this archive is for loading data. */
    __forceinline bool IsLoading() const { return ArIsLoading; }

    /** Sets ArIsError to true. Also sets error in the proxy archiver if one is wrapping this. */
    void SetError();

    /** Sets whether this archive is for loading data. */
    virtual void SetIsLoading(bool bInIsLoading);

    /** Sets whether this archive is for saving data. */
    virtual void SetIsSaving(bool bInIsSaving);

    /** Sets whether this archive is to persistent storage. */
    virtual void SetIsPersistent(bool bInIsPersistent);

    /** Returns true if data larger than 1 byte should be swapped to deal with endian mismatches. */
    __forceinline bool IsByteSwapping()
    {
#if PLATFORM_LITTLE_ENDIAN
        bool SwapBytes = ArForceByteSwapping;
#else
        bool SwapBytes = this->IsPersistent();
#endif
        return SwapBytes;
    }

protected:
    /** Whether this archive is for loading data. */
    uint8 ArIsLoading : 1;

    /** Whether this archive is for saving data. */
    uint8 ArIsSaving : 1;

    /** Whether this archive saves to persistent storage. This is also true for some intermediate archives like DuplicateObject that are expected to go to persistent storage but may be discarded */
    uint8 ArIsPersistent : 1;

private:
    /** Whether this archive contains errors, which means that further serialization is generally not safe */
    uint8 ArIsError : 1;

    /** Whether we should forcefully swap bytes. */
    uint8 ArForceByteSwapping : 1;

private:
    /** Linked list to all proxies */
    ArchiveState *NextProxy = nullptr;

    template <typename T>
    void ForEachState(T Func);
};

/**
 * Base class for archives that can be used for loading, saving, and garbage
 * collecting in a byte order neutral way.
 */
class Archive : public ArchiveState
{
public:
    using ArchiveState::SetError;
    using ArchiveState::TotalSize;

    // Serializes an unsigned 8-bit integer value from or into an archive.
    __forceinline friend Archive &operator<<(Archive &Ar, uint8 &Value)
    {
        Ar.Serialize(&Value, 1);
        return Ar;
    }

    // Serializes an unsigned 16-bit integer value from or into an archive.
    __forceinline friend Archive &operator<<(Archive &Ar, uint16 &Value)
    {
        Ar.ByteOrderSerialize(Value);
        return Ar;
    }

    // Serializes an unsigned 32-bit integer value from or into an archive.
    __forceinline friend Archive &operator<<(Archive &Ar, uint32 &Value)
    {
        Ar.ByteOrderSerialize(Value);
        return Ar;
    }

    // Serializes a unsigned 64-bit integer value from or into an archive.
    virtual void Serialize(void *V, int64 Length) = 0;

    // Used internally only to control the amount of generated code/type under control.
    template <typename T>
    Archive &ByteOrderSerialize(T &Value)
    {
        static_assert(!std::is_signed<T>::value, "To reduce the number of template instances, cast 'Value' to a uint16&, uint32& or uint64& prior to the call or use ByteOrderSerialize(void*, int32).");

        if (!IsByteSwapping()) // Most likely case (hot path)
        {
            Serialize(&Value, sizeof(T));
            return *this;
        }

        printf("Error: Need Byte Swapping %s %d\n", __FILE__, __LINE__);
        exit(-1);
        return *this;
        // return SerializeByteOrderSwapped(Value); // Slowest and unlikely path (but fastest than SerializeByteOrderSwapped(void*, int32)).
    }
};

/**
 * Serializes an enumeration value from or into an archive.
 *
 * @param Ar The archive to serialize from or to.
 * @param Value The value to serialize.
 */
template <class TEnum>
__forceinline Archive &operator<<(Archive &Ar, TEnumAsByte<TEnum> &Value)
{
    Ar.Serialize(&Value, 1);
    return Ar;
}