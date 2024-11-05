#pragma once

#include <cstdint>
#include <cstring>

/**
 * Traits class which tests if a type is a pointer.
 */
template <typename T>
struct TIsPointer
{
    enum
    {
        Value = false
    };
};
template <typename T>
struct TIsPointer<T *>
{
    enum
    {
        Value = true
    };
};
template <typename T>
struct TIsPointer<const T>
{
    enum
    {
        Value = TIsPointer<T>::Value
    };
};
template <typename T>
struct TIsPointer<volatile T>
{
    enum
    {
        Value = TIsPointer<T>::Value
    };
};
template <typename T>
struct TIsPointer<const volatile T>
{
    enum
    {
        Value = TIsPointer<T>::Value
    };
};

template <typename T>
static void ZeroVulkanStruct(T &Struct, int32_t VkStructureType)
{
    static_assert(!TIsPointer<T>::Value, "Don't use a pointer!");
    static_assert(offsetof(T, sType) == 0, "Assumes sType is the first member in the Vulkan type!");
    static_assert(sizeof(T::sType) == sizeof(int32_t), "Assumed sType is compatible with int32!");
    // Horrible way to coerce the compiler to not have to know what T::sType is so we can have this header not have to include vulkan.h
    (int32_t &)Struct.sType = VkStructureType;
    memset(((uint8_t *)&Struct) + sizeof(VkStructureType), 0, sizeof(T) - sizeof(VkStructureType));
}