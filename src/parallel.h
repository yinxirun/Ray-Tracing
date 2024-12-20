#pragma once
#include <functional>
#include <thread>
#include <mutex>
#include <cassert>
#include <condition_variable>

void ParallelInit();
void ParallelCleanup();
int NumSystemCores();

// Simple one-use barrier; ensures that multiple threads all reach a
// particular point of execution before allowing any of them to proceed
// past it.
//
// Note: this should be heap allocated and managed with a shared_ptr, where
// all threads that use it are passed the shared_ptr. This ensures that
// memory for the Barrier won't be freed until all threads have
// successfully cleared it.
class Barrier
{
public:
    Barrier(int count) : count(count) { assert(count > 0); }
    ~Barrier() { assert(count == 0); }
    void Wait();

private:
    std::mutex mutex;
    std::condition_variable cv;
    int count;
};

extern void ParallelFor(const std::function<void(int)> &func, int count, int chunkSize = 32);