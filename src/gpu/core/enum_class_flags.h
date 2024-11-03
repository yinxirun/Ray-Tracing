#pragma once

// Defines all bitwise operators for enum classes so it can be (mostly) used as a regular flags enum
#define ENUM_CLASS_FLAGS(Enum)                                                                                                          \
    inline Enum &operator|=(Enum &Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs | (__underlying_type(Enum))Rhs); }  \
    inline Enum &operator&=(Enum &Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs & (__underlying_type(Enum))Rhs); }  \
    inline Enum &operator^=(Enum &Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs ^ (__underlying_type(Enum))Rhs); }  \
    inline constexpr Enum operator|(Enum Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs | (__underlying_type(Enum))Rhs); } \
    inline constexpr Enum operator&(Enum Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs & (__underlying_type(Enum))Rhs); } \
    inline constexpr Enum operator^(Enum Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs ^ (__underlying_type(Enum))Rhs); } \
    inline constexpr bool operator!(Enum E) { return !(__underlying_type(Enum))E; }                                                     \
    inline constexpr Enum operator~(Enum E) { return (Enum) ~(__underlying_type(Enum))E; }

template <typename Enum>
constexpr bool EnumHasAnyFlags(Enum Flags, Enum Contains)
{
    using UnderlyingType = __underlying_type(Enum);
    return ((UnderlyingType)Flags & (UnderlyingType)Contains) != 0;
}

template<typename Enum>
constexpr bool EnumHasAllFlags(Enum Flags, Enum Contains)
{
	using UnderlyingType = __underlying_type(Enum);
	return ((UnderlyingType)Flags & (UnderlyingType)Contains) == (UnderlyingType)Contains;
}