#pragma once

#include <cassert>

#ifndef check
#define check(expr) assert(expr)
#endif

/**
 * Denotes code paths that should never be reached.
 */
#ifndef checkNoEntry
#define checkNoEntry() check(!"Enclosing block should never be called")
#endif

