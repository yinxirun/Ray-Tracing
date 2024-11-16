#pragma once
#include "Volk/volk.h"
#include <memory>
#include <vector>
#include "gpu/definitions.h"
#include "gpu/core/pixel_format.h"
#include "gpu/RHI/RHIAccess.h"
#include "gpu/RHI/RHIDefinitions.h"
#include "gpu/RHI/RHIResources.h"
#include "gpu/core/assertion_macros.h"

class Device;
class Viewport;
class CommandListContext;
class RHI;
class RHICommandListBase;
class ResourceCreateInfo;
class BufferDesc;
class Buffer;
class GraphicsPipelineState;
class GraphicsPipelineStateInitializer;
class RasterizerStateInitializer;
class RasterizerState;
class BlendStateInitializerRHI;
class BlendState;
class DepthStencilStateInitializerRHI;
class DepthStencilState;
class VertexDeclaration;
class VertexElement;
typedef std::vector<VertexElement> VertexDeclarationElementList;
class PixelShader;
class VertexShader;

extern RHI *rhi;

class RHI
{
public:
    // static RHI &Get() { return *rhi; }

    static RHI &Get()
    {
        static RHI vulkanRHI;
        return vulkanRHI;
    }

    /** Initialization constructor. */
    RHI();

    /* RHI Method */

    void Init();

    void Shutdown();

    // FlushType: Thread safe
    virtual std::shared_ptr<RasterizerState> CreateRasterizerState(const RasterizerStateInitializer &Initializer);

    // FlushType: Thread safe
    virtual std::shared_ptr<DepthStencilState> CreateDepthStencilState(const DepthStencilStateInitializerRHI &Initializer);

    // FlushType: Thread safe
    virtual std::shared_ptr<BlendState> CreateBlendState(const BlendStateInitializerRHI &Initializer);

    // FlushType: Wait RHI Thread
    virtual std::shared_ptr<VertexDeclaration> CreateVertexDeclaration(const VertexDeclarationElementList &Elements);

    virtual PixelShader *CreatePixelShader(std::vector<uint8> Code);

    virtual VertexShader *CreateVertexShader(std::vector<uint8> Code);

    CommandListContext *GetDefaultContext();

    std::shared_ptr<Viewport> CreateViewport(void *WindowHandle, uint32 SizeX, uint32 SizeY,
                                             bool bIsFullscreen, PixelFormat PreferredPixelFormat);

    // 288
    /**
     * Creates a graphics pipeline state object (PSO) that represents a complete gpu pipeline for rendering.
     * This function should be considered expensive to call at runtime and may cause hitches as pipelines are compiled.
     * @param Initializer - Descriptor object defining all the information needed to create the PSO, as well as behavior hints to the RHI.
     * @return FGraphicsPipelineStateRHIRef that can be bound for rendering; nullptr if the compilation fails.
     * CAUTION: On certain RHI implementations (eg, ones that do not support runtime compilation) a compilation failure is a Fatal error and this function will not return.
     * CAUTION: Even though this is marked as threadsafe, it is only valid to call from the render thread or the RHI thread. It need not be threadsafe unless the RHI support parallel translation.
     * CAUTION: Platforms that support RHIThread but don't actually have a threadsafe implementation must flush internally with FScopedRHIThreadStaller StallRHIThread(FRHICommandListExecutor::GetImmediateCommandList()); when the call is from the render thread
     */
    // FlushType: Thread safe
    virtual GraphicsPipelineState *CreateGraphicsPipelineState(const GraphicsPipelineStateInitializer &Initializer);

    /**
     * Creates a uniform buffer.  The contents of the uniform buffer are provided in a parameter, and are immutable.
     * CAUTION: Even though this is marked as threadsafe, it is only valid to call from the render thread or the RHI thread. Thus is need not be threadsafe on platforms that do not support or aren't using an RHIThread
     * @param Contents - A pointer to a memory block of size NumBytes that is copied into the new uniform buffer.
     * @param NumBytes - The number of bytes the uniform buffer should contain.
     * @return The new uniform buffer.
     */
    // FlushType: Thread safe, but varies depending on the RHI
    virtual std::shared_ptr<UniformBuffer> CreateUniformBuffer(const void *Contents, std::shared_ptr<const UniformBufferLayout> Layout, UniformBufferUsage Usage, UniformBufferValidation Validation);

    // 339
    virtual std::shared_ptr<Buffer> CreateBuffer(BufferDesc const &Desc, Access ResourceState, ResourceCreateInfo &CreateInfo);
    /*virtual void *LockBuffer(RHICommandListBase &RHICmdList, Buffer *Buffer, uint32 Offset, uint32 Size, ResourceLockMode LockMode);
    // FlushType: Flush RHI Thread
    virtual void UnlockBuffer(RHICommandListBase &RHICmdList, Buffer *Buffer);*/

    // 673
    //  Compute the hash of the state components of the PSO initializer for PSO Precaching (only hash data relevant for the RHI specific PSO)
    virtual uint64 ComputeStatePrecachePSOHash(const GraphicsPipelineStateInitializer &Initializer);

    // Compute the hash of the PSO initializer for PSO Precaching (only hash data relevant for the RHI specific PSO)
    virtual uint64 ComputePrecachePSOHash(const GraphicsPipelineStateInitializer &Initializer);

    // 715
    virtual void ResizeViewport(Viewport *Viewport, uint32 SizeX, uint32 SizeY,
                                bool bIsFullscreen, PixelFormat PreferredPixelFormat);
    // 919
    //  Buffer Lock/Unlock
    virtual void *LockBuffer_BottomOfPipe(class RHICommandListBase &RHICmdList, Buffer *Buffer, uint32 Offset, uint32 SizeRHI, ResourceLockMode LockMode);

    virtual void UnlockBuffer_BottomOfPipe(class RHICommandListBase &RHICmdList, Buffer *Buffer);

    /* Interface VulkanRHI Methond */

    /* VulkanRHI Methond */
    void InitInstance();
    inline VkInstance GetInstance() const { return instance; }
    inline Device *GetDevice() const { return device; }
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