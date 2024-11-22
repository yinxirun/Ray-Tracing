#pragma once

#include "gpu/vk/rhi.h"

extern RHI *rhi;

__forceinline CommandContext *GetDefaultContext() { return rhi->GetDefaultContext(); }

__forceinline std::shared_ptr<Viewport> CreateViewport(void *WindowHandle, uint32 SizeX, uint32 SizeY,
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

__forceinline GraphicsPipelineState *CreateGraphicsPipelineState(const GraphicsPipelineStateInitializer &Initializer)
{
    return rhi->CreateGraphicsPipelineState(Initializer);
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
