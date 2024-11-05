#pragma once
#include "Volk/volk.h"
#include <memory>
#include <vector>
#include "gpu/definitions.h"
#include "gpu/core/pixel_format.h"
#include "gpu/RHI/RHIAccess.h"

class Device;
class Viewport;
class CommandListContext;
class RHI;
class RHICommandListBase;
class ResourceCreateInfo;
class BufferDesc;
class Buffer;

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
                                             bool bIsFullscreen, PixelFormat PreferredPixelFormat);
    // 339
    virtual std::shared_ptr<Buffer> CreateBuffer(BufferDesc const &Desc, Access ResourceState, ResourceCreateInfo &CreateInfo);
    // 715
    virtual void ResizeViewport(Viewport *Viewport, uint32 SizeX, uint32 SizeY,
                                bool bIsFullscreen, PixelFormat PreferredPixelFormat);

    void InitInstance();

    /* Interface VulkanRHI Methond */

    /* VulkanRHI Methond */

    inline VkInstance GetInstance() const { return instance; }
    inline std::vector<Viewport *> &GetViewports() { return viewports; }
    inline uint32_t GetApiVersion() const { return apiVersion; }

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