#pragma once

#include "Volk/volk.h"
#include "command_buffer.h"

class Device;

class Queue
{
public:
    Queue(Device *InDevice, uint32_t InFamilyIndex)
        : queue(VK_NULL_HANDLE), device(InDevice), familyIndex(InFamilyIndex), queueIndex(0), LayoutManager(false, nullptr)
    {
        vkGetDeviceQueue(device->GetInstanceHandle(), familyIndex, queueIndex, &queue);
    }

    inline uint32_t GetFamilyIndex() const { return familyIndex; }

    void Submit(CmdBuffer* CmdBuffer, uint32 NumSignalSemaphores = 0, VkSemaphore* SignalSemaphores = nullptr);

    inline void GetLastSubmittedInfo(CmdBuffer *&OutCmdBuffer, uint64 &OutFenceCounter) const
    {
        //FScopeLock ScopeLock(&CS);
        OutCmdBuffer = LastSubmittedCmdBuffer;
        OutFenceCounter = LastSubmittedCmdBufferFenceCounter;
    }

    inline LayoutManager &GetLayoutManager() { return LayoutManager; }

private:
    Device *device;
    uint32_t familyIndex;
    uint32_t queueIndex;
    VkQueue queue;

    CmdBuffer* LastSubmittedCmdBuffer;
	uint64 LastSubmittedCmdBufferFenceCounter;

    // Last known layouts submitted on this queue, used for defrag
    LayoutManager LayoutManager;

    void UpdateLastSubmittedCommandBuffer(CmdBuffer* CmdBuffer);
};