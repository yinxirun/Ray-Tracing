#pragma once
namespace Math
{
    /** Divides two integers and rounds up */
    template <class T>
    static constexpr T DivideAndRoundUp(T Dividend, T Divisor)
    {
        return (Dividend + Divisor - 1) / Divisor;
    }
}