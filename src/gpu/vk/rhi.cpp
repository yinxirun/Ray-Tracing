#include "rhi.h"
#include <vector>
#include <stdexcept>
#include <iostream>
#include "GLFW/glfw3.h"
#include "common.h"
#include "device.h"
#include "viewport.h"
#include "context.h"

#define VULKAN_HAS_DEBUGGING_ENABLED 1

const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> instanceExtensions = {VK_KHR_SURFACE_EXTENSION_NAME};

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

std::vector<const char *> getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (VULKAN_HAS_DEBUGGING_ENABLED)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

VkBool32 debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                       void *pUserData)
{
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

/*********************************************************************************************************/

// Selects the device to us for the provided instance
static VkPhysicalDevice SelectPhysicalDevice(VkInstance InInstance)
{
    VkResult Result;

    uint32_t physicalDeviceCount = 0;
    Result = vkEnumeratePhysicalDevices(InInstance, &physicalDeviceCount, nullptr);
    if ((Result != VK_SUCCESS) || (physicalDeviceCount == 0))
    {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> physicalDevices;
    physicalDevices.resize(physicalDeviceCount);
    vkEnumeratePhysicalDevices(InInstance, &physicalDeviceCount, physicalDevices.data());

    struct FPhysicalDeviceInfo
    {
        FPhysicalDeviceInfo() = delete;
        FPhysicalDeviceInfo(uint32_t InOriginalIndex, VkPhysicalDevice InPhysicalDevice)
            : originalIndex(InOriginalIndex), physicalDevice(InPhysicalDevice)
        {
            ZeroVulkanStruct(physicalDeviceProperties2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
            ZeroVulkanStruct(physicalDeviceIDProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);
            physicalDeviceProperties2.pNext = &physicalDeviceIDProperties;
            vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties2);
        }

        uint32_t originalIndex;
        VkPhysicalDevice physicalDevice;
        VkPhysicalDeviceProperties2 physicalDeviceProperties2;
        VkPhysicalDeviceIDProperties physicalDeviceIDProperties;
    };
    std::vector<FPhysicalDeviceInfo> physicalDeviceInfos;
    physicalDeviceInfos.reserve(physicalDeviceCount);

    // Fill the array with each devices properties
    for (uint32_t index = 0; index < physicalDeviceCount; ++index)
    {
        physicalDeviceInfos.emplace_back(index, physicalDevices[index]);
    }

    return physicalDeviceInfos[0].physicalDevice;
}

RHI::RHI() : instance(VK_NULL_HANDLE), device(nullptr), drawingViewport(nullptr)
{
    CreateInstance();
    SelectDevice();
}

void RHI::Init()
{
    InitInstance();
}

void RHI::Shutdown()
{
    device->Destroy();
    delete device;
    device = nullptr;

#if VULKAN_HAS_DEBUGGING_ENABLED
    RemoveDebugLayerCallback();
#endif

    vkDestroyInstance(instance, nullptr);
}

CommandListContext *RHI::GetDefaultContext() { return &device->GetImmediateContext(); }

std::shared_ptr<Viewport> RHI::CreateViewport(void *WindowHandle, uint32 SizeX, uint32 SizeY,
                                              bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
{
    // Use a default pixel format if none was specified
    if (PreferredPixelFormat == PF_Unknown)
    {
        PreferredPixelFormat = PF_B8G8R8A8;
    }

    return std::shared_ptr<Viewport>(new Viewport(device, WindowHandle, SizeX, SizeY, PreferredPixelFormat));
}

void RHI::InitInstance()
{
    device->InitGPU();
}

void RHI::ResizeViewport(Viewport *viewport, uint32 sizeX, uint32 sizeY,
                         bool bIsFullscreen, EPixelFormat preferredPixelFormat)
{
    check(IsInGameThread());

    // Use a default pixel format if none was specified
    if (preferredPixelFormat == PF_Unknown)
    {
        preferredPixelFormat = PF_B8G8R8A8;
    }

    if (viewport->GetSizeXY() != IntVec2(sizeX, sizeY) || viewport->IsFullscreen() != bIsFullscreen)
    {
#ifdef PRINT_UNIMPLEMENT
        printf("RHI::Need multiple thread\n %s %d\n", __FILE__, __LINE__);
#endif
        device->SubmitCommandsAndFlushGPU();
        viewport->Resize(sizeX, sizeY, bIsFullscreen, preferredPixelFormat);
        device->SubmitCommandsAndFlushGPU();
    }
}

void RHI::CreateInstance()
{
    volkInitialize();
    apiVersion = VK_API_VERSION_1_3;

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = apiVersion;

    VkInstanceCreateInfo instInfo{};
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pApplicationInfo = &appInfo;

    this->instanceExtensions = getRequiredExtensions();
    instInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    instInfo.ppEnabledExtensionNames = instanceExtensions.data();

    this->instanceLayers = validationLayers;
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (VULKAN_HAS_DEBUGGING_ENABLED)
    {
        instInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
        instInfo.ppEnabledLayerNames = instanceLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        instInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
    }
    else
    {
        instInfo.enabledLayerCount = 0;
        instInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&instInfo, nullptr, &instance) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create instance!");
    }
    volkLoadInstance(instance);

#if VULKAN_HAS_DEBUGGING_ENABLED
    SetupDebugLayerCallback();
#endif
}

void RHI::SelectDevice()
{
    VkPhysicalDevice physicalDevice = SelectPhysicalDevice(instance);
    if (physicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }

    device = new Device(this, physicalDevice);
}

void RHI::SetupDebugLayerCallback()
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &messenger) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void RHI::RemoveDebugLayerCallback()
{
    DestroyDebugUtilsMessengerEXT(instance, messenger, nullptr);
}

RHI *globalRHI = nullptr;