#pragma once

#include <cassert>

#define check(expr) assert(expr)

#define checkf(expr, format, ...) CHECK_F_IMPL(expr, format, ##__VA_ARGS__)

#define CHECK_F_IMPL(expr, format, ...)    \
    {                                      \
        if ((!(expr)))                     \
        {                                  \
            printf(format, ##__VA_ARGS__); \
            assert(0);                     \
        }                                  \
    }

#define ensure(expr) assert(expr)

/**
 * Denotes code paths that should never be reached.
 */
#ifndef checkNoEntry
#define checkNoEntry() check(!"Enclosing block should never be called")
#endif
