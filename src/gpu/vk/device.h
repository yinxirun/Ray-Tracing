#pragma once
#include "Volk/volk.h"
#include <cstdint>
#include <cassert>
#include <vector>
#include <iostream>
#include "common.h"
#include <cstring>

class Queue;
class RHI;

extern const std::vector<const char *> deviceExtensions;
extern const std::vector<const char *> validationLayers;

class PhysicalDeviceFeatures
{
public:
    PhysicalDeviceFeatures()
    {
        memset(this, 0, sizeof(PhysicalDeviceFeatures));
    }

    void Query(VkPhysicalDevice PhysicalDevice, uint32_t APIVersion);

    VkPhysicalDeviceFeatures Core_1_0;
    VkPhysicalDeviceVulkan11Features Core_1_1;

private:
    // Anything above Core 1.1 cannot be assumed, they should only be used by the device at init time
    VkPhysicalDeviceVulkan12Features Core_1_2;
    VkPhysicalDeviceVulkan13Features Core_1_3;

    friend class FVulkanDevice;
};

class Device
{
public:
    Device(RHI *rhi, VkPhysicalDevice Gpu);

    void InitGPU();

    void CreateDevice(std::vector<const char *> &layers, std::vector<const char *> &extensions);

    void Destroy();

    inline VkDevice GetInstanceHandle() const
    {
        return device;
    }

    inline const VkPhysicalDeviceProperties &GetDeviceProperties() const
    {
        return gpuProps;
    }

private:
    RHI *rhi;

    VkDevice device;
    VkPhysicalDevice gpu;
    VkPhysicalDeviceProperties gpuProps;

    PhysicalDeviceFeatures physicalDeviceFeatures;
    std::vector<VkQueueFamilyProperties> queueFamilyProps;

    Queue *gfxQueue;
    Queue *computeQueue;
    Queue *transferQueue;
    Queue *presentQueue;
};