#pragma once

#include "Volk/volk.h"
#include "resources.h"
#include "gpu/core/serialization/memory_writer.h"
#include "gpu/core/misc/crc.h"
#include <string>

namespace VulkanRHI
{
    /**
     * Checks that the given result isn't a failure.  If it is, the application exits with an appropriate error message.
     * @param	Result - The result code to check
     * @param	Code - The code which yielded the result.
     * @param	VkFunction - Tested function name.
     * @param	Filename - The filename of the source file containing Code.
     * @param	Line - The line number of Code within Filename.
     */
    extern void VerifyVulkanResult(VkResult Result, const char *VkFuntion, const char *Filename, uint32_t Line);
}

#define VERIFYVULKANRESULT(VkFunction)                                                    \
    {                                                                                     \
        const VkResult ScopedResult = VkFunction;                                         \
        if (ScopedResult != VK_SUCCESS)                                                   \
        {                                                                                 \
            VulkanRHI::VerifyVulkanResult(ScopedResult, #VkFunction, __FILE__, __LINE__); \
        }                                                                                 \
    }

#define VERIFYVULKANRESULT_EXPANDED(VkFunction)                                           \
    {                                                                                     \
        const VkResult ScopedResult = VkFunction;                                         \
        if (ScopedResult < VK_SUCCESS)                                                    \
        {                                                                                 \
            VulkanRHI::VerifyVulkanResult(ScopedResult, #VkFunction, __FILE__, __LINE__); \
        }                                                                                 \
    }

template <typename T>
inline bool CopyAndReturnNotEqual(T &A, T B)
{
    const bool bOut = A != B;
    A = B;
    return bOut;
}

template <int Version>
class TDataKeyBase;

template <>
class TDataKeyBase<0>
{
protected:
    template <class DataReceiver>
    void GetData(DataReceiver &&ReceiveData)
    {
        std::vector<uint8> TempData;
        ReceiveData(TempData);
    }

    void SetData(const void *InData, uint32 InSize) {}

    void CopyDataDeep(TDataKeyBase &Result) const {}
    void CopyDataShallow(TDataKeyBase &Result) const {}
    bool IsDataEquals(const TDataKeyBase &Other) const { return true; }

protected:
    uint32 Hash = 0;
};

template <>
class TDataKeyBase<1>
{
protected:
    template <class DataReceiver>
    void GetData(DataReceiver &&ReceiveData)
    {
        EnsureDataStorage();
        ReceiveData(*Data);
    }

    void SetData(const void *InData, uint32 InSize)
    {
        EnsureDataStorage();
        Data->resize(InSize);
        memcpy(Data->data(), InData, InSize);
    }

    void CopyDataDeep(TDataKeyBase &Result) const
    {
        check(Data);
        Result.DataStorage = std::make_unique<std::vector<uint8>>(*Data);
        Result.Data = Result.DataStorage.get();
    }

    void CopyDataShallow(TDataKeyBase &Result) const
    {
        check(Data);
        Result.Data = Data;
    }

    bool IsDataEquals(const TDataKeyBase &Other) const
    {
        check(Data && Other.Data);
        check(Data->size() == Other.Data->size());
        check(memcmp(Data->data(), Other.Data->data(), Data->size()) == 0);
        return true;
    }

public:
    std::vector<uint8> &GetDataRef()
    {
        return *Data;
    }

private:
    void EnsureDataStorage()
    {
        if (!DataStorage)
        {
            DataStorage = std::make_unique<std::vector<uint8>>();
            Data = DataStorage.get();
        }
    }

protected:
    uint32 Hash = 0;
    std::vector<uint8> *Data = nullptr;

private:
    std::unique_ptr<std::vector<uint8>> DataStorage;
};

template <>
class TDataKeyBase<2> : public TDataKeyBase<1>
{
protected:
    bool IsDataEquals(const TDataKeyBase &Other) const
    {
        check(Data && Other.Data);
        return ((Data->size() == Other.Data->size()) &&
                (memcmp(Data->data(), Other.Data->data(), Data->size()) == 0));
    }
};

template <class Derived, bool AlwaysCompareData = false>
class TDataKey : public TDataKeyBase<AlwaysCompareData ? 2 : 1>
{
public:
    template <class ArchiveWriter>
    void GenerateFromArchive(ArchiveWriter &&WriteToArchive, int32 DataReserve = 0)
    {
        this->GetData([&](std::vector<uint8> &InData)
                      {
			MemoryWriter Ar(InData);

			InData.resize(DataReserve);
			WriteToArchive(Ar);

			this->Hash = MemCrc32(InData.data(), InData.size()); });
    }

    template <class ObjectType>
    void GenerateFromObject(const ObjectType &Object)
    {
        GenerateFromData(&Object, sizeof(Object));
    }

    void GenerateFromData(const void *InData, uint32 InSize)
    {
        this->SetData(InData, InSize);
        this->Hash = MemCrc32(InData, InSize);
    }

    uint32 GetHash() const
    {
        return this->Hash;
    }

    Derived CopyDeep() const
    {
        Derived Result;
        Result.Hash = this->Hash;
        this->CopyDataDeep(Result);
        return Result;
    }

    Derived CopyShallow() const
    {
        Derived Result;
        Result.Hash = this->Hash;
        this->CopyDataShallow(Result);
        return Result;
    }

    friend uint32 GetTypeHash(const Derived &Key)
    {
        return Key.Hash;
    }

    friend bool operator==(const Derived &A, const Derived &B)
    {
        return ((A.Hash == B.Hash) && A.IsDataEquals(B));
    }
};
