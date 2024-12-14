#pragma once

#include "vk/rhi.h"

extern RHI *rhi;

__forceinline CommandContext *GetDefaultContext() { return rhi->GetDefaultContext(); }

__forceinline std::shared_ptr<VulkanViewport> CreateViewport(void *WindowHandle, uint32 SizeX, uint32 SizeY,
                                                             bool bIsFullscreen, PixelFormat PreferredPixelFormat)
{
    return rhi->CreateViewport(WindowHandle, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
}

__forceinline std::shared_ptr<Buffer> CreateBuffer(BufferDesc const &Desc, Access ResourceState, ResourceCreateInfo &CreateInfo)
{
    return rhi->CreateBuffer(Desc, ResourceState, CreateInfo);
}

__forceinline void *LockBuffer_BottomOfPipe(RHICommandListBase &RHICmdList, Buffer *BufferRHI, uint32 Offset, uint32 Size, ResourceLockMode LockMode)
{
    return rhi->LockBuffer_BottomOfPipe(RHICmdList, BufferRHI, Offset, Size, LockMode);
}

__forceinline void UnlockBuffer_BottomOfPipe(RHICommandListBase &RHICmdList, Buffer *BufferRHI)
{
    return rhi->UnlockBuffer_BottomOfPipe(RHICmdList, BufferRHI);
}

__forceinline VertexShader *CreateVertexShader(std::vector<uint8> Code)
{
    return rhi->CreateVertexShader(Code);
}

__forceinline PixelShader *CreatePixelShader(std::vector<uint8> Code)
{
    return rhi->CreatePixelShader(Code);
}

__forceinline ComputeShader *CreateComputeShader(std::vector<uint8> Code)
{
    return rhi->CreateComputeShader(Code);
}

__forceinline GraphicsPipelineState *CreateGraphicsPipelineState(const GraphicsPipelineStateInitializer &Initializer)
{
    return rhi->CreateGraphicsPipelineState(Initializer);
}

__forceinline ComputePipelineState *CreateComputePipelineState(ComputeShader *shader)
{
    return rhi->CreateComputePipelineState(shader);
}

__forceinline std::shared_ptr<UniformBuffer> CreateUniformBuffer(const void *Contents, std::shared_ptr<const UniformBufferLayout> Layout, UniformBufferUsage Usage, UniformBufferValidation Validation)
{
    return rhi->CreateUniformBuffer(Contents, Layout, Usage, Validation);
}

__forceinline std::shared_ptr<SamplerState> CreateSamplerState(const SamplerStateInitializer &Initializer)
{
    return rhi->CreateSamplerState(Initializer);
}

__forceinline std::shared_ptr<Texture> CreateTexture(RHICommandListBase &RHICmdList, const TextureCreateDesc &CreateDesc)
{
    return rhi->CreateTexture(RHICmdList, CreateDesc);
}

__forceinline void *LockTexture2D(Texture *Texture, uint32 MipIndex, ResourceLockMode LockMode,
                                  uint32 &DestStride, bool bLockWithinMiptail, uint64 *OutLockedByteCount = nullptr)
{
    return rhi->LockTexture2D(Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail, OutLockedByteCount);
}

__forceinline void Unlock(Texture *Texture, uint32 MipIndex, bool bLockWithinMiptail)
{
    rhi->UnlockTexture2D(Texture, MipIndex, bLockWithinMiptail);
}

__forceinline void UpdateTexture2D(RHICommandListBase &RHICmdList, Texture *Texture, uint32 MipIndex,
                                   const struct UpdateTextureRegion2D &UpdateRegion, uint32 SourcePitch, const uint8 *SourceData)
{
    rhi->UpdateTexture2D(RHICmdList, Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
}