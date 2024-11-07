#pragma once
#include "gpu/definitions.h"
#include <type_traits>

__forceinline uint32 MurmurFinalize32(uint32 Hash)
{
	Hash ^= Hash >> 16;
	Hash *= 0x85ebca6b;
	Hash ^= Hash >> 13;
	Hash *= 0xc2b2ae35;
	Hash ^= Hash >> 16;
	return Hash;
}

/**
 * Combines two hash values to get a third.
 * Note - this function is not commutative.
 *
 * This function cannot change for backward compatibility reasons.
 * You may want to choose HashCombineFast for a better in-memory hash combining function.
 */
inline uint32 HashCombine(uint32 A, uint32 C)
{
	uint32 B = 0x9e3779b9;
	A += B;

	A -= B;
	A -= C;
	A ^= (C >> 13);
	B -= C;
	B -= A;
	B ^= (A << 8);
	C -= A;
	C -= B;
	C ^= (B >> 13);
	A -= B;
	A -= C;
	A ^= (C >> 12);
	B -= C;
	B -= A;
	B ^= (A << 16);
	C -= A;
	C -= B;
	C ^= (B >> 5);
	A -= B;
	A -= C;
	A ^= (C >> 3);
	B -= C;
	B -= A;
	B ^= (A << 10);
	C -= A;
	C -= B;
	C ^= (B >> 15);

	return C;
}

/// An unsigned integer the same size as a pointer
using UPTRINT = uint64;

inline uint32 PointerHash(const void *Key)
{
	// Ignoring the lower 4 bits since they are likely zero anyway.
	// Higher bits are more significant in 64 bit builds.
	const UPTRINT PtrInt = reinterpret_cast<UPTRINT>(Key) >> 4;
	return MurmurFinalize32((uint32)PtrInt);
}

template <
	typename ScalarType,
	std::enable_if_t<std::is_scalar_v<ScalarType> && !std::is_same_v<ScalarType, char *> && !std::is_same_v<ScalarType, const char *>> * = nullptr>
inline uint32 GetTypeHash(ScalarType Value)
{
	if constexpr (std::is_integral_v<ScalarType>)
	{
		if constexpr (sizeof(ScalarType) <= 4)
		{
			return Value;
		}
		else if constexpr (sizeof(ScalarType) == 8)
		{
			return (uint32)Value + ((uint32)(Value >> 32) * 23);
		}
		else if constexpr (sizeof(ScalarType) == 16)
		{
			const uint64 Low = (uint64)Value;
			const uint64 High = (uint64)(Value >> 64);
			return GetTypeHash(Low) ^ GetTypeHash(High);
		}
		else
		{
			static_assert(sizeof(ScalarType) == 0, "Unsupported integral type");
			return 0;
		}
	}
	else if constexpr (std::is_floating_point_v<ScalarType>)
	{
		if constexpr (std::is_same_v<ScalarType, float>)
		{
			return *(uint32 *)&Value;
		}
		else if constexpr (std::is_same_v<ScalarType, double>)
		{
			return GetTypeHash(*(uint64 *)&Value);
		}
		else
		{
			static_assert(sizeof(ScalarType) == 0, "Unsupported floating point type");
			return 0;
		}
	}
	else if constexpr (std::is_enum_v<ScalarType>)
	{
		return GetTypeHash((__underlying_type(ScalarType))Value);
	}
	else if constexpr (std::is_pointer_v<ScalarType>)
	{
		// Once the TCHAR* deprecations below are removed, we want to prevent accidental string hashing, so this static_assert should be commented back in
		// static_assert(!TIsCharType<std::remove_pointer_t<ScalarType>>::Value, "Pointers to string types should use a PointerHash() or FCrc::Stricmp_DEPRECATED() call depending on requirements");

		return PointerHash(Value);
	}
	else
	{
		static_assert(sizeof(ScalarType) == 0, "Unsupported scalar type");
		return 0;
	}
}
