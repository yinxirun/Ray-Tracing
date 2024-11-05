#pragma once
#include "Volk/volk.h"
#define VK_NO_PROTOTYPES
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vma/vk_mem_alloc.h"
#include "gpu/definitions.h"
#include <atomic>
#include <cassert>
#include <vector>
class Device;
class CmdBuffer;

enum class EDelayAcquireImageType
{
    None,         // acquire next image on frame start
    DelayAcquire, // acquire next image just before presenting, rendering is done to intermediate image which is copied to real backbuffer
    LazyAcquire,  // acquire next image on first use
};

extern EDelayAcquireImageType GVulkanDelayAcquireImage;

namespace VulkanRHI
{
    class FenceManager;

    // Custom ref counting
    class RefCount
    {
    public:
        RefCount() : numRefs(0) {}

        virtual ~RefCount()
        {
            assert(numRefs.load() == 0);
        }

        inline uint32_t AddRef()
        {
            numRefs.fetch_add(1);
            uint32_t newValue = numRefs.load();
            assert(newValue > 0);
            return (uint32_t)newValue;
        }

        inline uint32_t Release()
        {
            numRefs.fetch_sub(1);
            uint32_t newValue = numRefs.load();
            if (newValue == 0)
            {
                delete this;
            }
            assert(newValue >= 0);
            return (uint32_t)newValue;
        }

        inline uint32_t GetRefCount() const
        {
            int32_t value = numRefs.load();
            assert(value >= 0);
            return (uint32_t)value;
        }

    private:
        std::atomic<uint32_t> numRefs;
    };

    class DeviceChild
    {
    public:
        DeviceChild(Device *InDevice) : device(InDevice) {}

        Device *GetParent() const
        {
            // Has to have one if we are asking for it...
            check(device);
            return device;
        }

    protected:
        Device *const device;
    };

    class StagingBuffer : public RefCount
    {
    public:
        StagingBuffer(Device *InDevice);
        VkBuffer GetHandle() const;
        void *GetMappedPointer();
        uint32 GetSize() const;
        void FlushMappedMemory();

    protected:
        Device *device;
        VmaAllocation allocation;

        VkBuffer buffer;
        VkMemoryPropertyFlagBits MemoryReadFlags;
        VmaAllocationInfo allocationInfo;

        // Owner maintains lifetime
        virtual ~StagingBuffer();

        void Destroy();

        friend class StagingManager;
    };

    class StagingManager
    {
    public:
        StagingManager() : device(nullptr) {}
        ~StagingManager();

        void Init(Device *InDevice) { device = InDevice; }
        void Deinit();

        StagingBuffer *AcquireBuffer(uint32 Size,
                                     VkBufferUsageFlags InUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VkMemoryPropertyFlagBits InMemoryReadFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        // Sets pointer to nullptr
        void ReleaseBuffer(CmdBuffer *CmdBuffer, StagingBuffer *&StagingBuffer);

    protected:
        Device *device;

        std::vector<StagingBuffer *> UsedStagingBuffers;
    };

    class Fence
    {
    public:
        Fence(Device *InDevice, FenceManager *InOwner, bool bCreateSignaled);

        inline VkFence GetHandle() const { return handle; }

        inline bool IsSignaled() const { return state == EState::Signaled; }

        FenceManager *GetOwner() { return owner; }

    protected:
        VkFence handle;

        enum class EState
        {
            // Initial state
            NotReady,
            // After GPU processed it
            Signaled,
        };

        EState state;

        FenceManager *owner;
        // Only owner can delete!
        ~Fence();
        friend class FenceManager;
    };

    class FenceManager
    {
    public:
        FenceManager() : device(nullptr) {}

        ~FenceManager();

        void Init(Device *InDevice);

        void Deinit();

        Fence *AllocateFence(bool bCreateSignaled = false);

        inline bool IsFenceSignaled(Fence *fence)
        {
            if (fence->IsSignaled())
            {
                return true;
            }
            return CheckFenceState(fence);
        }

        // Returns false if it timed out
        bool WaitForFence(Fence *fence, uint64 imeInNanoseconds);

        void ResetFence(Fence *);

        // Sets it to nullptr
        void ReleaseFence(Fence *&Fence);

        // Sets it to nullptr
        void WaitAndReleaseFence(Fence *&Fence, uint64 TimeInNanoseconds);

    protected:
        Device *device;
        std::vector<Fence *> freeFences;
        std::vector<Fence *> usedFences;

        // Returns true if signaled
        bool CheckFenceState(Fence *fence);

        void DestroyFence(Fence *fence);
    };

    class Semaphore : public RefCount
    {
    public:
        Semaphore(Device &InDevice);
        Semaphore(Device &InDevice, const VkSemaphore &InExternalSemaphore);
        virtual ~Semaphore();

        inline VkSemaphore GetHandle() const
        {
            return semaphoreHandle;
        }

        inline bool IsExternallyOwned() const
        {
            return bExternallyOwned;
        }

    private:
        Device &device;
        VkSemaphore semaphoreHandle;
        bool bExternallyOwned;
    };
}