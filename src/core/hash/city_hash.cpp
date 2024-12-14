#include "city_hash.h"
#include <cstring>
#include <cstdlib>
static uint64 UNALIGNED_LOAD64(const char *p)
{
    uint64 result;
    memcpy(&result, p, sizeof(result));
    return result;
}

static uint32 UNALIGNED_LOAD32(const char *p)
{
    uint32 result;
    memcpy(&result, p, sizeof(result));
    return result;
}

#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)

// 大段还是小段
#ifdef WORDS_BIGENDIAN
#define uint32_in_expected_order(x) (bswap_32(x))
#define uint64_in_expected_order(x) (bswap_64(x))
#else
#define uint32_in_expected_order(x) (x)
#define uint64_in_expected_order(x) (x)
#endif

// 111
static uint64 Fetch64(const char *p)
{
    return uint64_in_expected_order(UNALIGNED_LOAD64(p));
}

static uint32 Fetch32(const char *p)
{
    return uint32_in_expected_order(UNALIGNED_LOAD32(p));
}

template <typename T>
void SwapValues(T &a, T &b)
{
    T c = a;
    a = b;
    b = c;
}

namespace CityHash_Internal
{
    // Some primes between 2^63 and 2^64 for various uses.
    static const uint64 k0 = 0xc3a5c85c97cb3127ULL;
    static const uint64 k1 = 0xb492b66fbe98f273ULL;
    static const uint64 k2 = 0x9ae16a3b2f90404fULL;

    // Magic numbers for 32-bit hashing.  Copied from Murmur3.
    static const uint32 c1 = 0xcc9e2d51;
    static const uint32 c2 = 0x1b873593;
}

// Bitwise right rotate.  Normally this will compile to a single
// instruction, especially if the shift is a manifest constant.
static uint64 Rotate(uint64 val, int shift)
{
    // Avoid shifting by 64: doing so yields an undefined result.
    return shift == 0 ? val : ((val >> shift) | (val << (64 - shift)));
}

static uint64 ShiftMix(uint64 val)
{
    return val ^ (val >> 47);
}

static uint64 HashLen16(uint64 u, uint64 v)
{
    return CityHash128to64({u, v});
}

static uint64 HashLen16(uint64 u, uint64 v, uint64 mul)
{
    // Murmur-inspired hashing.
    uint64 a = (u ^ v) * mul;
    a ^= (a >> 47);
    uint64 b = (v ^ a) * mul;
    b ^= (b >> 47);
    b *= mul;
    return b;
}

// 299
static uint64 HashLen0to16(const char *s, uint32 len)
{
    using namespace CityHash_Internal;

    if (len >= 8)
    {
        uint64 mul = k2 + len * 2;
        uint64 a = Fetch64(s) + k2;
        uint64 b = Fetch64(s + len - 8);
        uint64 c = Rotate(b, 37) * mul + a;
        uint64 d = (Rotate(a, 25) + b) * mul;
        return HashLen16(c, d, mul);
    }
    if (len >= 4)
    {
        uint64 mul = k2 + len * 2;
        uint64 a = Fetch32(s);
        return HashLen16(len + (a << 3), Fetch32(s + len - 4), mul);
    }
    if (len > 0)
    {
        uint8 a = s[0];
        uint8 b = s[len >> 1];
        uint8 c = s[len - 1];
        uint32 y = static_cast<uint32>(a) + (static_cast<uint32>(b) << 8);
        uint32 z = len + (static_cast<uint32>(c) << 2);
        return ShiftMix(y * k2 ^ z * k0) * k2;
    }
    return k2;
}

// This probably works well for 16-byte strings as well, but it may be overkill
// in that case.
static uint64 HashLen17to32(const char *s, uint32 len)
{
    using namespace CityHash_Internal;

    uint64 mul = k2 + len * 2;
    uint64 a = Fetch64(s) * k1;
    uint64 b = Fetch64(s + 8);
    uint64 c = Fetch64(s + len - 8) * mul;
    uint64 d = Fetch64(s + len - 16) * k2;
    return HashLen16(Rotate(a + b, 43) + Rotate(c, 30) + d,
                     a + Rotate(b + k2, 18) + c, mul);
}

// Return a 16-byte hash for 48 bytes.  Quick and dirty.
// Callers do best to use "random-looking" values for a and b.
static Uint128_64 WeakHashLen32WithSeeds(
    uint64 w, uint64 x, uint64 y, uint64 z, uint64 a, uint64 b)
{
    a += w;
    b = Rotate(b + a + z, 21);
    uint64 c = a;
    a += x;
    a += y;
    b += Rotate(a, 44);
    return {(a + z), (b + c)};
}

// Return a 16-byte hash for s[0] ... s[31], a, and b.  Quick and dirty.
static Uint128_64 WeakHashLen32WithSeeds(
    const char *s, uint64 a, uint64 b)
{
    return WeakHashLen32WithSeeds(Fetch64(s),
                                  Fetch64(s + 8),
                                  Fetch64(s + 16),
                                  Fetch64(s + 24),
                                  a,
                                  b);
}

// Return an 8-byte hash for 33 to 64 bytes.
static uint64 HashLen33to64(const char *s, uint32 len)
{
    using namespace CityHash_Internal;

    uint64 mul = k2 + len * 2;
    uint64 a = Fetch64(s) * k2;
    uint64 b = Fetch64(s + 8);
    uint64 c = Fetch64(s + len - 24);
    uint64 d = Fetch64(s + len - 32);
    uint64 e = Fetch64(s + 16) * k2;
    uint64 f = Fetch64(s + 24) * 9;
    uint64 g = Fetch64(s + len - 8);
    uint64 h = Fetch64(s + len - 16) * mul;
    uint64 u = Rotate(a + g, 43) + (Rotate(b, 30) + c) * 9;
    uint64 v = ((a + g) ^ d) + f + 1;
    uint64 w = bswap_64((u + v) * mul) + h;
    uint64 x = Rotate(e + f, 42) + c;
    uint64 y = (bswap_64((v + w) * mul) + g) * mul;
    uint64 z = e + f + c;
    a = bswap_64((x + z) * mul + y) + b;
    b = ShiftMix((z + a) * mul + d + h) * mul;
    return b + x;
}

uint64 CityHash64(const char *s, uint32 len)
{
    using namespace CityHash_Internal;

    if (len <= 32)
    {
        if (len <= 16)
        {
            return HashLen0to16(s, len);
        }
        else
        {
            return HashLen17to32(s, len);
        }
    }
    else if (len <= 64)
    {
        return HashLen33to64(s, len);
    }

    // For strings over 64 bytes we hash the end first, and then as we
    // loop we keep 56 bytes of state: v, w, x, y, and z.
    uint64 x = Fetch64(s + len - 40);
    uint64 y = Fetch64(s + len - 16) + Fetch64(s + len - 56);
    uint64 z = HashLen16(Fetch64(s + len - 48) + len, Fetch64(s + len - 24));
    Uint128_64 v = WeakHashLen32WithSeeds(s + len - 64, len, z);
    Uint128_64 w = WeakHashLen32WithSeeds(s + len - 32, y + k1, x);
    x = x * k1 + Fetch64(s);

    // Decrease len to the nearest multiple of 64, and operate on 64-byte chunks.
    len = (len - 1) & ~static_cast<uint32>(63);
    do
    {
        x = Rotate(x + y + v.lo + Fetch64(s + 8), 37) * k1;
        y = Rotate(y + v.hi + Fetch64(s + 48), 42) * k1;
        x ^= w.hi;
        y += v.lo + Fetch64(s + 40);
        z = Rotate(z + w.lo, 33) * k1;
        v = WeakHashLen32WithSeeds(s, v.hi * k1, x + w.lo);
        w = WeakHashLen32WithSeeds(s + 32, z + w.hi, y + Fetch64(s + 16));
        SwapValues(z, x);
        s += 64;
        len -= 64;
    } while (len != 0);
    return HashLen16(HashLen16(v.lo, w.lo) + ShiftMix(y) * k1 + z,
                     HashLen16(v.hi, w.hi) + x);
}