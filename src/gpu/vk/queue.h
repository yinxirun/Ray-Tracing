#pragma once

#include "Volk/volk.h"

class Device;

class Queue
{
public:
    Queue(Device *InDevice, uint32_t InFamilyIndex) : queue(VK_NULL_HANDLE), device(InDevice), familyIndex(InFamilyIndex), queueIndex(0)
    {
        vkGetDeviceQueue(device->GetInstanceHandle(), familyIndex, queueIndex, &queue);
    }

private:
    Device *device;
    uint32_t familyIndex;
    uint32_t queueIndex;
    VkQueue queue;
};