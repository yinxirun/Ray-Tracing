#include "parallel.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <condition_variable>

static std::vector<std::thread> threads;
static bool shutdownThreads = false;
class ParallelForLoop;

/// 工作队列的头节点
static ParallelForLoop *workList = nullptr;
static std::mutex workListMutex;
static std::condition_variable workListCondition;

int NumSystemCores()
{
    return std::thread::hardware_concurrency();
}

int MaxThreadIndex()
{
    return NumSystemCores();
}

class ParallelForLoop
{
public:
    ParallelForLoop(std::function<void(int)> func1D, int64_t maxIndex, int chunkSize)
        : func1D(std::move(func1D)), maxIndex(maxIndex), chunkSize(chunkSize) {}

private:
    std::function<void(int)> func1D;
    std::function<void(int)> func2D;
    const int64_t maxIndex;
    const int chunkSize;

    int64_t nextIndex = 0;
    int activeWorkers = 0;
    ParallelForLoop *next = nullptr;
    int nX = -1;

    bool Finished() const
    {
        return nextIndex >= maxIndex && activeWorkers == 0;
    }

    friend void ParallelFor(const std::function<void(int)> &func, int count, int chunkSize);
    friend void workerThreadFunc(int tIndex, std::shared_ptr<Barrier> barrier);
};

void Barrier::Wait()
{
    std::unique_lock<std::mutex> lock(mutex);
    assert(count > 0);
    if (--count == 0)
        // This is the last thread to reach the barrier; wake up all of the
        // other ones before exiting.
        cv.notify_all();
    else
        // Otherwise there are still threads that haven't reached it. Give
        // up the lock and wait to be notified.
        cv.wait(lock, [this]
                { return count == 0; });
}

thread_local int ThreadIndex;

void workerThreadFunc(int tIndex, std::shared_ptr<Barrier> barrier)
{
    // std::cout << "Started execution in worker thread " << tIndex;
    ThreadIndex = tIndex;

    // The main thread sets up a barrier so that it can be sure that all
    // workers have called ProfilerWorkerThreadInit() before it continues
    // (and actually starts the profiling system).
    barrier->Wait();

    // Release our reference to the Barrier so that it's freed once all of
    // the threads have cleared it.
    barrier.reset();

    std::unique_lock<std::mutex> lock(workListMutex);
    while (!shutdownThreads)
    {
        if (!workList)
        {
            // Sleep until there are more tasks to run
            workListCondition.wait(lock);
        }
        else
        {
            // Get work from _workList_ and run loop iterations
            ParallelForLoop &loop = *workList;

            // Run a chunk of loop iterations for _loop_

            // Find the set of loop iterations to run next
            int64_t indexStart = loop.nextIndex;
            int64_t indexEnd = std::min(indexStart + loop.chunkSize, loop.maxIndex);

            // Update _loop_ to reflect iterations this thread will run
            loop.nextIndex = indexEnd;
            if (loop.nextIndex == loop.maxIndex)
                workList = loop.next;
            loop.activeWorkers++;

            // Run loop indices in _[indexStart, indexEnd)_
            lock.unlock();
            for (int64_t index = indexStart; index < indexEnd; ++index)
            {
                if (loop.func1D)
                {
                    loop.func1D(index);
                }
            }
            lock.lock();

            // Update _loop_ to reflect completion of iterations
            loop.activeWorkers--;
            if (loop.Finished())
                workListCondition.notify_all();
        }
    }
    // std::cout << "Exiting worker thread " << tIndex;
}

void ParallelFor(const std::function<void(int)> &func, int count, int chunkSize)
{
    // Run iterations immediately if not using threads or if count is small
    auto nThreads = std::thread::hardware_concurrency();
    if (nThreads == 1 || count < chunkSize)
    {
        for (int i = 0; i < count; ++i)
            func(i);
        return;
    }

    // Create and enqueue ParallelForLoop for this loop
    ParallelForLoop loop(func, count, chunkSize);
    workListMutex.lock();
    loop.next = workList;
    workList = &loop;
    workListMutex.unlock();

    // Notify worker threads of work to be done
    std::unique_lock<std::mutex> lock(workListMutex);
    workListCondition.notify_all();

    // Help out with parallel loop iterations in the current thread
    while (!loop.Finished())
    { // Run a chunk of loop iterations for _loop_

        // Find the set of loop iterations to run next
        int64_t indexStart = loop.nextIndex;
        int64_t indexEnd = std::min(indexStart + loop.chunkSize, loop.maxIndex);

        // Update _loop_ to reflect iterations this thread will run
        loop.nextIndex = indexEnd;
        if (loop.nextIndex == loop.maxIndex)
            workList = loop.next;
        loop.activeWorkers++;

        // Run loop indices in _[indexStart, indexEnd)_
        lock.unlock();
        for (int64_t index = indexStart; index < indexEnd; ++index)
        {
            if (loop.func1D)
            {
                loop.func1D(index);
            }
        }
        lock.lock();

        // Update _loop_ to reflect completion of iterations
        loop.activeWorkers--;
    }
}

void ParallelInit()
{
    assert(threads.size() == 0);
    int nThreads = MaxThreadIndex();
    ThreadIndex = 0;

    // Create a barrier so that we can be sure all worker threads get past
    // their call to ProfilerWorkerThreadInit() before we return from this
    // function.  In turn, we can be sure that the profiling system isn't
    // started until after all worker threads have done that.
    std::shared_ptr<Barrier> barrier = std::make_shared<Barrier>(nThreads);

    // Launch one fewer worker thread than the total number we want doing
    // work, since the main thread helps out, too.
    for (int i = 0; i < nThreads - 1; ++i)
        threads.push_back(std::thread(workerThreadFunc, i + 1, barrier));

    barrier->Wait();
}

void ParallelCleanup()
{
    if (threads.empty())
        return;

    {
        std::lock_guard<std::mutex> lock(workListMutex);
        shutdownThreads = true;
        workListCondition.notify_all();
    }

    for (std::thread &thread : threads)
        thread.join();
    threads.erase(threads.begin(), threads.end());
    shutdownThreads = false;
}