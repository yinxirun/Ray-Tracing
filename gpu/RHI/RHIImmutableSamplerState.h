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
    ImmutableSamplerState() { Reset(); }

    void Reset()
    {
        for (uint32 Index = 0; Index < MaxImmutableSamplers; ++Index)
        {
            ImmutableSamplers[Index] = nullptr;
        }
    }

    bool operator==(const ImmutableSamplerState &rhs) const
    {
        return ImmutableSamplers == rhs.ImmutableSamplers;
    }

    bool operator!=(const ImmutableSamplerState &rhs) const
    {
        return ImmutableSamplers != rhs.ImmutableSamplers;
    }

    TImmutableSamplers ImmutableSamplers;
};