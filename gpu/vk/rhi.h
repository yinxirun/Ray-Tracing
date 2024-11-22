#pragma once
#include "Volk/volk.h"
#include <memory>
#include <vector>
#include "gpu/definitions.h"
#include "gpu/core/pixel_format.h"
#include "gpu/RHI/RHIAccess.h"
#include "gpu/RHI/RHIDefinitions.h"
#include "gpu/RHI/RHIResources.h"
#include "gpu/RHI/RHIPipeline.h"
#include "gpu/RHI/multi_gpu.h"
#include "gpu/core/assertion_macros.h"

class Device;
class Viewport;
class CommandContext;
class CommandListContext;
class ComputeContext;
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
class VulkanUniformBuffer;

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
    virtual std::shared_ptr<SamplerState> CreateSamplerState(const SamplerStateInitializer &Initializer);

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

    virtual CommandContext *GetDefaultContext();

    virtual ComputeContext *GetCommandContext(RHIPipeline Pipeline, RHIGPUMask GPUMask);

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

    virtual void UpdateUniformBuffer(RHICommandListBase &RHICmdList, UniformBuffer *UniformBufferRHI, const void *Contents);

    // 339
    virtual std::shared_ptr<Buffer> CreateBuffer(BufferDesc const &Desc, Access ResourceState, ResourceCreateInfo &CreateInfo);

    virtual std::shared_ptr<Texture> CreateTexture(RHICommandListBase &RHICmdList, const TextureCreateDesc &CreateDesc);

    /**
     * Locks an RHI texture's mip-map for read/write operations on the CPU
     * @param Texture - the RHI texture resource to lock, must not be 0
     * @param MipIndex - index of the mip level to lock
     * @param LockMode - Whether to lock the texture read-only instead of write-only
     * @param DestStride - output to retrieve the textures row stride (pitch)
     * @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
     * @return pointer to the CPU accessible resource data
     */
    // FlushType: Flush RHI Thread
    virtual void *LockTexture2D(Texture *Texture, uint32 MipIndex, ResourceLockMode LockMode, uint32 &DestStride, bool bLockWithinMiptail, uint64 *OutLockedByteCount = nullptr);

    /**
     * Unlocks a previously locked RHI texture resource
     * @param Texture - the RHI texture resource to unlock, must not be 0
     * @param MipIndex - index of the mip level to unlock
     * @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
     */
    // FlushType: Flush RHI Thread
    virtual void UnlockTexture2D(Texture *Texture, uint32 MipIndex, bool bLockWithinMiptail)
    {
        InternalUnlockTexture2D(false, Texture, MipIndex, bLockWithinMiptail);
    }

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

    void InternalUnlockTexture2D(bool bFromRenderingThread, Texture *Texture, uint32 MipIndex, bool bLockWithinMiptail);

    /// @brief 不能再render pass中调用。我的修改导致的？
    void UpdateUniformBuffer(RHICommandListBase &RHICmdList, VulkanUniformBuffer *UniformBuffer, const void *Contents);
};