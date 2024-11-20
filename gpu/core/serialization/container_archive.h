#pragma once

#include "archive.h"
#include <vector>
#include <type_traits>

/// @brief 序列化/反序列化 std::vector
template <typename ElementType>
Archive &operator<<(Archive &ar, std::vector<ElementType> &vec)
{
    uint32 serializeNum = ar.IsLoading() ? 0 : vec.size();

    ar << serializeNum;

    if (serializeNum == 0)
    {
        // if we are loading, then we have to reset the size to 0, in case it isn't currently 0
        if (ar.IsLoading())
        {
            vec.clear();
        }
        return ar;
    }

    check(serializeNum >= 0);

    if (!ar.IsError() && serializeNum > 0)
    {
        if (ar.IsLoading())
        {
            // Required for resetting ArrayNum
            vec.resize(serializeNum);

            for (uint32 i = 0; i < serializeNum; i++)
            {
                ar << vec[i];
            }
        }
        else
        {
            for (uint32 i = 0; i < serializeNum; i++)
            {
                ar << vec[i];
            }
        }
    }
    else
    {
        ar.SetError();
    }

    return ar;
}