#pragma once

#include <array>

class SamplerState;

/** Maximum number of immutable samplers in a PSO. */
enum
{
    MaxImmutableSamplers = 2
};

struct ImmutableSamplerState
{
    using TImmutableSamplers = std::array<SamplerState *, MaxImmutableSamplers>;
    TImmutableSamplers ImmutableSamplers;
};