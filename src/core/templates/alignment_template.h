#pragma once

#include <type_traits>
#include "definitions.h"

/**
 * Aligns a value to the nearest higher multiple of 'Alignment', which must be a power of two.
 *
 * @param  Val        The value to align.
 * @param  Alignment  The alignment value, must be a power of two.
 *
 * @return The value aligned up to the specified alignment.
 */
template <typename T>
__forceinline constexpr T Align(T Val, uint64 Alignment)
{
    static_assert(std::is_integral<T>::value || std::is_pointer<T>::value, "Align expects an integer or pointer type");

    return (T)(((uint64)Val + Alignment - 1) & ~(Alignment - 1));
}

/**
 * Aligns a value to the nearest higher multiple of 'Alignment'.
 *
 * @param  Val        The value to align.
 * @param  Alignment  The alignment value, can be any arbitrary value.
 *
 * @return The value aligned up to the specified alignment.
 */
template <typename T>
__forceinline constexpr T AlignArbitrary(T Val, uint64 Alignment)
{
	static_assert(std::is_integral<T>::value || std::is_pointer<T>::value, "AlignArbitrary expects an integer or pointer type");

	return (T)((((uint64)Val + Alignment - 1) / Alignment) * Alignment);
}
