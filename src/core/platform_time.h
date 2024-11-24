#pragma once
#include <ctime>

class PlatformTime
{
public:
    static double Seconds()
    {
        return clock() / 1000.0;
    }
};