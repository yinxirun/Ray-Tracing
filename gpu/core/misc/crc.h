#pragma once
#include "gpu/definitions.h"

/**
 * Aligns a value to the nearest higher multiple of 'Alignment', which must be a power of two.
 *
 * @param  Val        The value to align.
 * @param  Alignment  The alignment value, must be a power of two.
 *
 * @return The value aligned up to the specified alignment.
 */
template <typename T>
constexpr T Align(T Val, uint64 Alignment)
{
	return (T)(((uint64)Val + Alignment - 1) & ~(Alignment - 1));
}

uint32 MemCrc32(const void *InData, int32 Length, uint32 CRC = 0);

uint32 MemCrc_DEPRECATED(const void *InData, int32 Length, uint32 CRC = 0);