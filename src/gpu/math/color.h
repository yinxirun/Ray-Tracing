#pragma once

/**
 * A linear, 32-bit/component floating point RGBA color.
 */
struct LinearColor
{
    union
    {
        struct
        {
            float R, G, B, A;
        };
    };
    
    constexpr LinearColor(float InR, float InG, float InB, float InA = 1.0f) : R(InR), G(InG), B(InB), A(InA) {}

    static const LinearColor Black;
};