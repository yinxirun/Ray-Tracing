#pragma once

#define NUM_GPU 1

/** A mask where each bit is a GPU index. Can not be empty so that non SLI platforms can optimize it to be always 1.  */
struct RHIGPUMask
{
private:
    uint32 GPUMask;

    __forceinline explicit RHIGPUMask(uint32 InGPUMask) : GPUMask(InGPUMask) { check(InGPUMask != 0); }

public:
    __forceinline RHIGPUMask() : RHIGPUMask(RHIGPUMask::GPU0()) {}

    __forceinline bool operator==(const RHIGPUMask &Rhs) const { return GPUMask == Rhs.GPUMask; }
    __forceinline static const RHIGPUMask GPU0() { return RHIGPUMask(1); }
    __forceinline static const RHIGPUMask All() { return RHIGPUMask((1 << NUM_GPU) - 1); }
};
