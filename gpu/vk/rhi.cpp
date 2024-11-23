#include "rhi.h"
#include <vector>
#include <stdexcept>
#include <iostream>
#include "GLFW/glfw3.h"
#include "common.h"
#include "device.h"
#include "viewport.h"
#include "context.h"
#include "pipeline.h"
#include "state.h"
#include "util.h"
#include "configuration.h"
#include "gpu/RHI/RHIResources.h"
#include "gpu/core/hash/city_hash.h"
#include "gpu/core/misc/secure_hash.h"

#define VULKAN_HAS_DEBUGGING_ENABLED 1

const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#if VULKAN_HAS_DEBUGGING_ENABLED
const std::vector<const char *> extensions = {VK_KHR_SURFACE_EXTENSION_NAME, "VK_KHR_win32_surface", VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
#else
const std::vector<const char *> extensions = {VK_KHR_SURFACE_EXTENSION_NAME, "VK_KHR_win32_surface"};
#endif

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
    {
        for (auto &Pair : device->SamplerMap)
        {
            VulkanSamplerState *SamplerState = (VulkanSamplerState *)Pair.second.get();
            vkDestroySampler(device->GetInstanceHandle(), SamplerState->sampler, VULKAN_CPU_ALLOCATOR);
        }
        device->SamplerMap.clear();
    }

    device->Destroy();
    delete device;
    device = nullptr;

#if VULKAN_HAS_DEBUGGING_ENABLED
    RemoveDebugLayerCallback();
#endif

    vkDestroyInstance(instance, nullptr);
}

std::shared_ptr<RasterizerState> RHI::CreateRasterizerState(const RasterizerStateInitializer &Initializer)
{
    return std::make_shared<VulkanRasterizerState>(Initializer);
}

std::shared_ptr<DepthStencilState> RHI::CreateDepthStencilState(const DepthStencilStateInitializer &Initializer)
{
    return std::make_shared<VulkanDepthStencilState>(Initializer);
}

std::shared_ptr<BlendState> RHI::CreateBlendState(const BlendStateInitializerRHI &Initializer)
{
    return std::make_shared<VulkanBlendState>(Initializer);
}

CommandContext *RHI::GetDefaultContext() { return &device->GetImmediateContext(); }

GraphicsPipelineState *RHI::CreateGraphicsPipelineState(const GraphicsPipelineStateInitializer &Initializer)
{
    return device->PipelineStateCache->CreateGraphicsPipelineState(Initializer);
}

std::shared_ptr<Viewport> RHI::CreateViewport(void *WindowHandle, uint32 SizeX, uint32 SizeY,
                                              bool bIsFullscreen, PixelFormat PreferredPixelFormat)
{
    // Use a default pixel format if none was specified
    if (PreferredPixelFormat == PF_Unknown)
    {
        PreferredPixelFormat = PF_B8G8R8A8;
    }

    return std::shared_ptr<Viewport>(new Viewport(device, WindowHandle, SizeX, SizeY, PreferredPixelFormat));
}

void *RHI::LockBuffer_BottomOfPipe(RHICommandListBase &RHICmdList, Buffer *BufferRHI, uint32 Offset, uint32 Size, ResourceLockMode LockMode)
{
    VulkanMultiBuffer *buffer = static_cast<VulkanMultiBuffer *>(BufferRHI);
    return buffer->Lock(RHICmdList, LockMode, Size, Offset);
}

void RHI::UnlockBuffer_BottomOfPipe(RHICommandListBase &RHICmdList, Buffer *BufferRHI)
{
    VulkanMultiBuffer *buffer = static_cast<VulkanMultiBuffer *>(BufferRHI);
    buffer->Unlock(*static_cast<CommandListContext *>(rhi->GetDefaultContext()));
}

void RHI::InitInstance()
{
    device->InitGPU();
}

// If you modify this function bump then GetPrecachePSOHashVersion, this will invalidate any previous uses of the hash.
// i.e. pre-existing PSO caches must be rebuilt.
uint64 RHI::ComputeStatePrecachePSOHash(const GraphicsPipelineStateInitializer &Initializer)
{
    struct FHashKey
    {
        uint32 VertexDeclaration;
        uint32 VertexShader;
        uint32 PixelShader;
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
        uint32 GeometryShader;
#endif // PLATFORM_SUPPORTS_GEOMETRY_SHADERS
#if PLATFORM_SUPPORTS_MESH_SHADERS
        uint32 MeshShader;
#endif // PLATFORM_SUPPORTS_MESH_SHADERS
        uint32 BlendState;
        uint32 RasterizerState;
        uint32 DepthStencilState;
        uint32 ImmutableSamplerState;

        uint32 MultiViewCount : 8;
        uint32 DrawShadingRate : 8;
        uint32 PrimitiveType : 8;
        uint32 bDepthBounds : 1;
        uint32 bHasFragmentDensityAttachment : 1;
        uint32 Unused : 6;
    } HashKey;

    memset(&HashKey, 0, sizeof(FHashKey));

    HashKey.VertexDeclaration = Initializer.BoundShaderState.VertexDeclarationRHI ? Initializer.BoundShaderState.VertexDeclarationRHI->GetPrecachePSOHash() : 0;
    HashKey.VertexShader = Initializer.BoundShaderState.GetVertexShader() ? GetTypeHash(Initializer.BoundShaderState.GetVertexShader()->GetHash()) : 0;
    HashKey.PixelShader = Initializer.BoundShaderState.GetPixelShader() ? GetTypeHash(Initializer.BoundShaderState.GetPixelShader()->GetHash()) : 0;
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    HashKey.GeometryShader = Initializer.BoundShaderState.GetGeometryShader() ? GetTypeHash(Initializer.BoundShaderState.GetGeometryShader()->GetHash()) : 0;
#endif
#if PLATFORM_SUPPORTS_MESH_SHADERS
    HashKey.MeshShader = Initializer.BoundShaderState.GetMeshShader() ? GetTypeHash(Initializer.BoundShaderState.GetMeshShader()->GetHash()) : 0;
#endif

    BlendStateInitializerRHI BlendStateInitializerRHI;
    if (Initializer.BlendState && Initializer.BlendState->GetInitializer(BlendStateInitializerRHI))
    {
        HashKey.BlendState = GetTypeHash(BlendStateInitializerRHI);
    }
    RasterizerStateInitializer RasterizerStateInitializerRHI;
    if (Initializer.RasterizerState && Initializer.RasterizerState->GetInitializer(RasterizerStateInitializerRHI))
    {
        HashKey.RasterizerState = GetTypeHash(RasterizerStateInitializerRHI);
    }
    DepthStencilStateInitializer DepthStencilStateInitializerRHI;
    if (Initializer.DepthStencilState && Initializer.DepthStencilState->GetInitializer(DepthStencilStateInitializerRHI))
    {
        HashKey.DepthStencilState = GetTypeHash(DepthStencilStateInitializerRHI);
    }

    // Ignore immutable samplers for now
    // HashKey.ImmutableSamplerState = GetTypeHash(ImmutableSamplerState);

    HashKey.MultiViewCount = Initializer.MultiViewCount;
    HashKey.DrawShadingRate = Initializer.ShadingRate;
    HashKey.PrimitiveType = Initializer.PrimitiveType;
    HashKey.bDepthBounds = Initializer.bDepthBounds;
    HashKey.bHasFragmentDensityAttachment = Initializer.bHasFragmentDensityAttachment;

    uint64 PrecachePSOHash = CityHash64((const char *)&HashKey, sizeof(FHashKey));

    return PrecachePSOHash;
}

uint64 RHI::ComputePrecachePSOHash(const GraphicsPipelineStateInitializer &Initializer)
{
    // When compute precache PSO hash we assume a valid state precache PSO hash is already provided
    uint64 StatePrecachePSOHash = Initializer.StatePrecachePSOHash;
    if (StatePrecachePSOHash == 0)
    {
        StatePrecachePSOHash = ComputeStatePrecachePSOHash(Initializer);
    }

    // checkf(StatePrecachePSOHash != 0, TEXT("Initializer should have a valid state precache PSO hash set when computing the full initializer PSO hash"));
    check(StatePrecachePSOHash != 0);

    // All members which are not part of the state objects
    struct NonStateHashKey
    {
        uint64 StatePrecachePSOHash;

        PrimitiveType PrimitiveType;
        uint32 RenderTargetsEnabled;
        GraphicsPipelineStateInitializer::TRenderTargetFormats RenderTargetFormats;
        GraphicsPipelineStateInitializer::TRenderTargetFlags RenderTargetFlags;
        PixelFormat DepthStencilTargetFormat;
        TextureCreateFlags DepthStencilTargetFlag;
        RenderTargetLoadAction DepthTargetLoadAction;
        RenderTargetStoreAction DepthTargetStoreAction;
        RenderTargetLoadAction StencilTargetLoadAction;
        RenderTargetStoreAction StencilTargetStoreAction;
        ExclusiveDepthStencil DepthStencilAccess;
        uint16 NumSamples;
        SubpassHint SubpassHint;
        uint8 SubpassIndex;
        ConservativeRasterization ConservativeRasterization;
        bool bDepthBounds;
        uint8 MultiViewCount;
        bool bHasFragmentDensityAttachment;
        VRSShadingRate ShadingRate;
    } HashKey;

    memset(&HashKey, 0, sizeof(NonStateHashKey));

    HashKey.StatePrecachePSOHash = StatePrecachePSOHash;

    HashKey.PrimitiveType = Initializer.PrimitiveType;
    HashKey.RenderTargetsEnabled = Initializer.RenderTargetsEnabled;
    HashKey.RenderTargetFormats = Initializer.RenderTargetFormats;
    HashKey.RenderTargetFlags = Initializer.RenderTargetFlags;
    HashKey.DepthStencilTargetFormat = Initializer.DepthStencilTargetFormat;
    HashKey.DepthStencilTargetFlag = Initializer.DepthStencilTargetFlag;
    HashKey.DepthTargetLoadAction = Initializer.DepthTargetLoadAction;
    HashKey.DepthTargetStoreAction = Initializer.DepthTargetStoreAction;
    HashKey.StencilTargetLoadAction = Initializer.StencilTargetLoadAction;
    HashKey.StencilTargetStoreAction = Initializer.StencilTargetStoreAction;
    HashKey.DepthStencilAccess = Initializer.DepthStencilAccess;
    HashKey.NumSamples = Initializer.NumSamples;
    HashKey.SubpassHint = Initializer.SubpassHint;
    HashKey.SubpassIndex = Initializer.SubpassIndex;
    HashKey.ConservativeRasterization = Initializer.ConservativeRasterization;
    HashKey.bDepthBounds = Initializer.bDepthBounds;
    HashKey.MultiViewCount = Initializer.MultiViewCount;
    HashKey.bHasFragmentDensityAttachment = Initializer.bHasFragmentDensityAttachment;
    HashKey.ShadingRate = Initializer.ShadingRate;

    // TODO: check if any RT flags actually affect PSO in VK
    for (TextureCreateFlags &Flags : HashKey.RenderTargetFlags)
    {
        Flags = Flags & GraphicsPipelineStateInitializer::RelevantRenderTargetFlagMask;
    }

    return CityHash64((const char *)&HashKey, sizeof(NonStateHashKey));
}

void RHI::ResizeViewport(Viewport *viewport, uint32 sizeX, uint32 sizeY,
                         bool bIsFullscreen, PixelFormat preferredPixelFormat)
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

    this->instanceExtensions = extensions;
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