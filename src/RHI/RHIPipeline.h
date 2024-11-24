#pragma once

#include "core/enum_class_flags.h"
#include "definitions.h"

enum class RHIPipeline : uint8
{
    Graphics = 1 << 0,
    AsyncCompute = 1 << 1,

    None = 0,
    All = Graphics | AsyncCompute,
    Num = 2
};
ENUM_CLASS_FLAGS(RHIPipeline)

