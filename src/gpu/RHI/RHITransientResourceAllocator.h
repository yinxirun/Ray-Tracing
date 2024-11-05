#pragma once

#include "gpu/definitions.h"

/** The base class for a platform heap implementation. Transient resources are placed on the heap at specific
 *  byte offsets. Each heap additionally contains a cache of RHI transient resources, each with its own RHI
 *  resource and cache of RHI views. The lifetime of the resource cache is tied to the heap.
 */
class RHITransientHeap
{
};

/** Represents an allocation from the transient heap. */
struct RHITransientHeapAllocation
{
    bool IsValid() const { return Size != 0; }

    // Transient heap which made the allocation.
    RHITransientHeap *Heap = nullptr;

    // Size of the allocation made from the allocator (aligned).
    uint64 Size = 0;

    // Offset in the transient heap; front of the heap starts at 0.
    uint64 Offset = 0;

    // Number of bytes of padding were added to the offset.
    uint32 AlignmentPad = 0;
};