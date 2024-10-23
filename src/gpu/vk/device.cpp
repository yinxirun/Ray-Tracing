#include "device.h"
#include "common.h"
#include "queue.h"
#include <iostream>
#include "rhi.h"

const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

void PhysicalDeviceFeatures::Query(VkPhysicalDevice PhysicalDevice, uint32_t APIVersion)
{
    VkPhysicalDeviceFeatures2 PhysicalDeviceFeatures2;
    ZeroVulkanStruct(PhysicalDeviceFeatures2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);

    PhysicalDeviceFeatures2.pNext = &Core_1_1;
    Core_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

    if (APIVersion >= VK_API_VERSION_1_2)
    {
        Core_1_1.pNext = &Core_1_2;
        Core_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    }

    if (APIVersion >= VK_API_VERSION_1_3)
    {
        Core_1_2.pNext = &Core_1_3;
        Core_1_3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    }

    vkGetPhysicalDeviceFeatures2(PhysicalDevice, &PhysicalDeviceFeatures2);

    // Copy features into old struct for convenience
    Core_1_0 = PhysicalDeviceFeatures2.features;
}

Device::Device(RHI *rhi, VkPhysicalDevice Gpu) : device(VK_NULL_HANDLE), gpu(Gpu), gfxQueue(nullptr), computeQueue(nullptr), transferQueue(nullptr), presentQueue(nullptr)
{
    this->rhi = rhi;
    {
        VkPhysicalDeviceProperties2KHR PhysicalDeviceProperties2;
        ZeroVulkanStruct(PhysicalDeviceProperties2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR);
        vkGetPhysicalDeviceProperties2(Gpu, &PhysicalDeviceProperties2);
        gpuProps = PhysicalDeviceProperties2.properties;
    }

    printf("- DeviceName: %s\n", gpuProps.deviceName);
    printf("- API=%d.%d.%d (0x%x) Driver=0x%x VendorId=0x%x\n", VK_VERSION_MAJOR(gpuProps.apiVersion), VK_VERSION_MINOR(gpuProps.apiVersion),
           VK_VERSION_PATCH(gpuProps.apiVersion), gpuProps.apiVersion, gpuProps.driverVersion, gpuProps.vendorID);
    printf("- Max Descriptor Sets Bound %d, Timestamps %d\n", gpuProps.limits.maxBoundDescriptorSets, gpuProps.limits.timestampComputeAndGraphics);
}

void Device::InitGPU()
{
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueCount, nullptr);
    assert(queueCount >= 1);

    queueFamilyProps.resize(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueCount, queueFamilyProps.data());

    // Query base features
    physicalDeviceFeatures.Query(gpu, rhi->GetApiVersion());

    // Setup layers and extensions
    std::vector<const char *> extensions = deviceExtensions;
    std::vector<const char *> layers = validationLayers;

    CreateDevice(layers, extensions);
}

void Device::CreateDevice(std::vector<const char *> &layers, std::vector<const char *> &extensions)
{
    // Setup extension and layer info
    VkDeviceCreateInfo DeviceInfo;
    ZeroVulkanStruct(DeviceInfo, VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);

    DeviceInfo.pEnabledFeatures = &physicalDeviceFeatures.Core_1_0;

    DeviceInfo.enabledExtensionCount = extensions.size();
    DeviceInfo.ppEnabledExtensionNames = extensions.data();

    DeviceInfo.enabledLayerCount = layers.size();
    DeviceInfo.ppEnabledLayerNames = (DeviceInfo.enabledLayerCount > 0) ? layers.data() : nullptr;

    // Setup Queue info
    std::vector<VkDeviceQueueCreateInfo> QueueFamilyInfos;
    int32_t GfxQueueFamilyIndex = -1;
    int32_t ComputeQueueFamilyIndex = -1;
    int32_t TransferQueueFamilyIndex = -1;
    printf("Found %d Queue Families\n", queueFamilyProps.size());
    uint32_t NumPriorities = 0;
    for (int32_t FamilyIndex = 0; FamilyIndex < queueFamilyProps.size(); ++FamilyIndex)
    {
        const VkQueueFamilyProperties &CurrProps = queueFamilyProps[FamilyIndex];

        bool bIsValidQueue = false;
        if ((CurrProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
        {
            if (GfxQueueFamilyIndex == -1)
            {
                GfxQueueFamilyIndex = FamilyIndex;
                bIsValidQueue = true;
            }
            else
            {
                // #todo-rco: Support for multi-queue/choose the best queue!
            }
        }

        if ((CurrProps.queueFlags & VK_QUEUE_COMPUTE_BIT) == VK_QUEUE_COMPUTE_BIT)
        {
            if (ComputeQueueFamilyIndex == -1 && GfxQueueFamilyIndex != FamilyIndex)
            {
                ComputeQueueFamilyIndex = FamilyIndex;
                bIsValidQueue = true;
            }
        }

        if ((CurrProps.queueFlags & VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT)
        {
            // Prefer a non-gfx transfer queue
            if (TransferQueueFamilyIndex == -1 && (CurrProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) != VK_QUEUE_GRAPHICS_BIT && (CurrProps.queueFlags & VK_QUEUE_COMPUTE_BIT) != VK_QUEUE_COMPUTE_BIT)
            {
                TransferQueueFamilyIndex = FamilyIndex;
                bIsValidQueue = true;
            }
        }

        if (!bIsValidQueue)
        {
            printf("Skipping unnecessary Queue Family %d: %d queues\n", FamilyIndex, CurrProps.queueCount);
            continue;
        }

        int32_t QueueIndex = QueueFamilyInfos.size();
        QueueFamilyInfos.push_back(VkDeviceQueueCreateInfo{});
        VkDeviceQueueCreateInfo &CurrQueue = QueueFamilyInfos[QueueIndex];
        CurrQueue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        CurrQueue.queueFamilyIndex = FamilyIndex;
        CurrQueue.queueCount = CurrProps.queueCount;
        NumPriorities += CurrProps.queueCount;
        printf("Initializing Queue Family %d: %d queues\n", FamilyIndex, CurrProps.queueCount);
    }

    std::vector<float> QueuePriorities;
    QueuePriorities.resize(NumPriorities);
    float *CurrentPriority = QueuePriorities.data();
    for (int32_t Index = 0; Index < QueueFamilyInfos.size(); ++Index)
    {
        VkDeviceQueueCreateInfo &CurrQueue = QueueFamilyInfos[Index];
        CurrQueue.pQueuePriorities = CurrentPriority;

        const VkQueueFamilyProperties &CurrProps = queueFamilyProps[CurrQueue.queueFamilyIndex];
        for (int32_t QueueIndex = 0; QueueIndex < (int32_t)CurrProps.queueCount; ++QueueIndex)
        {
            *CurrentPriority++ = 1.0f;
        }
    }
    DeviceInfo.queueCreateInfoCount = QueueFamilyInfos.size();
    DeviceInfo.pQueueCreateInfos = QueueFamilyInfos.data();

    // Create the device
    VkResult Result = vkCreateDevice(gpu, &DeviceInfo, nullptr, &device);
    if (Result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device!");
    }

    // Create Graphics Queue, here we submit command buffers for execution
    gfxQueue = new Queue(this, GfxQueueFamilyIndex);
    if (ComputeQueueFamilyIndex == -1)
    {
        // If we didn't find a dedicated Queue, use the default one
        ComputeQueueFamilyIndex = GfxQueueFamilyIndex;
    }
    computeQueue = new Queue(this, ComputeQueueFamilyIndex);
    if (TransferQueueFamilyIndex == -1)
    {
        // If we didn't find a dedicated Queue, use the default one
        TransferQueueFamilyIndex = ComputeQueueFamilyIndex;
    }
    transferQueue = new Queue(this, TransferQueueFamilyIndex);
}

void Device::Destroy()
{
    delete transferQueue;
    delete computeQueue;
    delete gfxQueue;

    vkDestroyDevice(device, nullptr);
    device = VK_NULL_HANDLE;
}