#pragma once

#include "gpu/definitions.h"

struct Uint128_64
{
	inline Uint128_64(uint64 InLo, uint64 InHi) : lo(InLo), hi(InHi) {}
	uint64 lo;
	uint64 hi;
};

// Hash function for a byte array.
uint64 CityHash64(const char *buf, uint32 len);

// Hash 128 input bits down to 64 bits of output.
// This is intended to be a reasonably good hash function.
inline uint64 CityHash128to64(const Uint128_64& x) {
	// Murmur-inspired hashing.
	const uint64 kMul = 0x9ddfea08eb382d69ULL;
	uint64 a = (x.lo ^ x.hi) * kMul;
	a ^= (a >> 47);
	uint64 b = (x.hi ^ a) * kMul;
	b ^= (b >> 47);
	b *= kMul;
	return b;
}