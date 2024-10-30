#pragma once
#include "Volk/volk.h"
#include <memory>
#include <vector>
#include "gpu/definitions.h"
#include "gpu/core/pixel_format.h"

class Device;
class Viewport;
class CommandListContext;

class RHI;

extern RHI *globalRHI;

class RHI
{
public:
    static RHI &Get() { return *globalRHI; }

    /** Initialization constructor. */
    RHI();

    /* RHI Method */

    void Init();

    void Shutdown();

    CommandListContext *GetDefaultContext();

    std::shared_ptr<Viewport> CreateViewport(void *WindowHandle, uint32 SizeX, uint32 SizeY,
                                             bool bIsFullscreen, EPixelFormat PreferredPixelFormat);

    void InitInstance();

    inline uint32_t GetApiVersion() const { return apiVersion; }

    /* Interface VulkanRHI Methond */

    /* VulkanRHI Methond */

protected:
    uint32_t apiVersion = 0;
    VkInstance instance;
    std::vector<const char *> instanceExtensions;
    std::vector<const char *> instanceLayers;

    Device *device;

    /** A list of all viewport RHIs that have been created. */
    std::vector<Viewport *> viewports;

    /** The viewport which is currently being drawn. */
    std::shared_ptr<Viewport> drawingViewport;

    void CreateInstance();
    void SelectDevice();

    friend class CommandListContext;
    friend class Viewport;

    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    void SetupDebugLayerCallback();
    void RemoveDebugLayerCallback();
};

inline bool UseVulkanDescriptorCache() { return false; }