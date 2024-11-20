#pragma once

#include "gpu/definitions.h"

namespace CrossCompiler
{
    enum class PackedTypeIndex : int8
    {
        HighP = 0,
        MediumP = 1,
        LowP = 2,
        Int = 3,
        Uint = 4,
        Sampler = 5,
        Image = 6,

        Max = 7,
        Invalid = -1,
    };
}