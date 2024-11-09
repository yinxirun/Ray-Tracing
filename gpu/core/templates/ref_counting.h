#pragma once

/**
 * Like FRefCountedObject, but internal ref count is thread safe
 */
class ThreadSafeRefCountedObject
{
public:
    __forceinline bool IsValid() const
    {
        return true;
    }
};