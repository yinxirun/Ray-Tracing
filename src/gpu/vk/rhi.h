#pragma once
#include "Volk/volk.h"
#include <memory>
#include <vector>

class Device;
class Viewport;

class RHI
{
public:
    /** Initialization constructor. */
    RHI();

    virtual void Init();

    virtual void Shutdown();

    void InitInstance();

    inline uint32_t GetApiVersion() const { return apiVersion; }

private:
    uint32_t apiVersion = 0;
    VkInstance instance;
    std::vector<const char *> instanceExtensions;
    std::vector<const char *> instanceLayers;

    Device *device;

    /** The viewport which is currently being drawn. */
    std::shared_ptr<Viewport> drawingViewport;

    void CreateInstance();
    void SelectDevice();

    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    void SetupDebugLayerCallback();
	void RemoveDebugLayerCallback();

};