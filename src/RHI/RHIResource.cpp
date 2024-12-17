#include "RHIResources.h"
#include "RHIDefinitions.h"
#include "RHIGlobals.h"
#include "dynamic_rhi.h"
#include <atomic>

RHIResource::RHIResource(ERHIResourceType InResourceType) : ResourceType(InResourceType) {}

RHIResource::~RHIResource()
{
    assert(numRefs.load() == 0);
}

ViewDesc::FBuffer::ViewInfo ViewDesc::FBuffer::GetViewInfo(Buffer *TargetBuffer) const
{
    check(TargetBuffer);
    checkf(bufferType != BufferType::Unknown, "A buffer type must be specified when creating a buffer view. Use SetType() or SetTypeFromBuffer().");

    BufferDesc const &Desc = TargetBuffer->GetDesc();
    if (Desc.IsNull())
    {
        ViewInfo Info = {};
        Info.bNullView = true;
        return Info;
    }

    ViewInfo Info = {};
    Info.BufferType = bufferType;
    Info.Format = Format;

    // Find the correct stride, and do some validation.
    switch (Info.BufferType)
    {
    default:
        checkNoEntry();
        [[fallthrough]];

    case BufferType::Typed:
        checkf(!EnumHasAnyFlags(Desc.Usage, BUF_StructuredBuffer | BUF_AccelerationStructure), "Cannot create typed views of structured buffers, or ray tracing acceleration structures.");
        checkf(Format != PF_Unknown, "Format cannot be unknown for typed buffers.");
        checkf(Stride == 0, "Do not specify a stride for typed buffer views.");
        check((OffsetInBytes % GetMinimumAlignmentForBufferBackedSRV(Info.Format)) == 0);

        // Stride is determined by the format
        Info.StrideInBytes = GPixelFormats[Info.Format].BlockBytes;
        break;

    case BufferType::Structured:
        checkf(EnumHasAnyFlags(Desc.Usage, BUF_StructuredBuffer), "The buffer descriptor is not a structured buffer, so is incompatible with this view type.");
        checkf(Format == PF_Unknown, "Structured buffer views should not specify a format.");

        // Stride is taken from the view, or the underlying buffer if not provided.
        Info.StrideInBytes = Stride == 0 ? Desc.Stride : Stride;
        checkf(Info.StrideInBytes > 0, "Stride for structured buffers must be set by the view, or on the underlying buffer resource.");
        checkf((OffsetInBytes % Info.StrideInBytes) == 0, "OffsetInBytes (%d) must be a multiple of element stride (%d).", OffsetInBytes, Info.StrideInBytes);
        break;

    case BufferType::AccelerationStructure:
        checkf(EnumHasAnyFlags(Desc.Usage, BUF_AccelerationStructure), "The buffer descriptor is not a ray tracing acceleration structure, so is incompatible with this view type.");
        checkf(Format == PF_Unknown, "Acceleration structure views should not specify a format.");
        checkf(Stride == 0, "Do not specify a stride for acceleration structure views.");

        // Treat acceleration structures as a byte array.
        Info.StrideInBytes = 1;
        break;

    case BufferType::Raw:
        checkf(GRHIGlobals.SupportsRawViewsForAnyBuffer || EnumHasAnyFlags(Desc.Usage, BUF_ByteAddressBuffer), "The current RHI does not support raw access to buffers created without the BUF_ByteAddressBuffer usage flag.");
        checkf(Format == PF_Unknown, "Raw buffer views should not specify a format.");
        checkf(Stride == 0, "Do not specify a stride for raw buffer views.");
        checkf((OffsetInBytes % 16) == 0, "The byte offset of raw views must be a multiple of 16 (specified offset: %d).", OffsetInBytes);

        // Raw buffers are always an array of 32-bit ints.
        Info.StrideInBytes = sizeof(uint32);
        Info.Format = PF_Unknown;
        break;
    }

    checkf(OffsetInBytes < Desc.Size, "Buffer byte offset (%d) is out of bounds (size: %d).", OffsetInBytes, Desc.Size);
    Info.OffsetInBytes = OffsetInBytes;

    // OffsetInBytes == 0 && NumElements == 0 is a special case to mean "whole resource". If offset is non-zero, we need the caller to pass the required number of elements, except for acceleration structures.
    checkf(Info.BufferType == BufferType::AccelerationStructure || (OffsetInBytes == 0 || NumElements > 0), "NumElements field must be non-zero if a byte offset is used.");

    // When NumElements is zero, use "whole buffer".
    Info.NumElements = NumElements == 0 ? (Desc.Size - OffsetInBytes) / Info.StrideInBytes : NumElements;
    Info.SizeInBytes = Info.NumElements * Info.StrideInBytes;

    checkf(Info.OffsetInBytes + Info.SizeInBytes <= Desc.Size,
           "The bounds of the view (offset: %d, size in bytes: %d, stride: %d, num elements: %d) exceeds the size of the underlying buffer (%d bytes).",
           Info.OffsetInBytes,
           Info.SizeInBytes,
           Info.StrideInBytes,
           Info.NumElements,
           Desc.Size);

    check(Info.Format == PF_Unknown || GPixelFormats[Info.Format].Supported);

    return Info;
}

ViewDesc::BufferUAV::ViewInfo ViewDesc::BufferUAV::GetViewInfo(Buffer *TargetBuffer) const
{
    check(ViewType == ViewType::BufferUAV);
    ViewInfo Info = {FBuffer::GetViewInfo(TargetBuffer)};

    // @todo checks to see if these flags are valid
    Info.bAtomicCounter = bAtomicCounter;
    Info.bAppendBuffer = bAppendBuffer;

    return Info;
}

ViewDesc::BufferSRV::ViewInfo ViewDesc::BufferSRV::GetViewInfo(Buffer *TargetBuffer) const
{
    check(ViewType == ViewType::BufferSRV);
    return ViewInfo{FBuffer::GetViewInfo(TargetBuffer)};
}

ViewableResource::ViewableResource(ERHIResourceType InResourceType, Access InAccess)
    : RHIResource(InResourceType), TrackedAccess(InAccess)
{
}

Texture::Texture(const TextureCreateDesc &InDesc) : ViewableResource(RRT_Texture, InDesc.InitialState), textureDesc(InDesc)
{
}

void RenderPassInfo::ConvertToRenderTargetsInfo(SetRenderTargetsInfo &OutRTInfo) const
{
    for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
    {
        if (!ColorRenderTargets[Index].RenderTarget)
        {
            break;
        }

        OutRTInfo.ColorRenderTarget[Index].texture = ColorRenderTargets[Index].RenderTarget;
        RenderTargetLoadAction LoadAction = GetLoadAction(ColorRenderTargets[Index].Action);
        OutRTInfo.ColorRenderTarget[Index].LoadAction = LoadAction;
        OutRTInfo.ColorRenderTarget[Index].StoreAction = GetStoreAction(ColorRenderTargets[Index].Action);
        OutRTInfo.ColorRenderTarget[Index].ArraySliceIndex = ColorRenderTargets[Index].ArraySlice;
        OutRTInfo.ColorRenderTarget[Index].MipIndex = ColorRenderTargets[Index].MipIndex;
        ++OutRTInfo.NumColorRenderTargets;

        OutRTInfo.bClearColor |= (LoadAction == RenderTargetLoadAction::EClear);

        if (ColorRenderTargets[Index].ResolveTarget)
        {
            OutRTInfo.bHasResolveAttachments = true;
            OutRTInfo.ColorResolveRenderTarget[Index] = OutRTInfo.ColorRenderTarget[Index];
            OutRTInfo.ColorResolveRenderTarget[Index].texture = ColorRenderTargets[Index].ResolveTarget;
        }
    }

    RenderTargetActions DepthActions = GetDepthActions(DepthStencilRenderTarget.Action);
    RenderTargetActions StencilActions = GetStencilActions(DepthStencilRenderTarget.Action);
    RenderTargetLoadAction DepthLoadAction = GetLoadAction(DepthActions);
    RenderTargetStoreAction DepthStoreAction = GetStoreAction(DepthActions);
    RenderTargetLoadAction StencilLoadAction = GetLoadAction(StencilActions);
    RenderTargetStoreAction StencilStoreAction = GetStoreAction(StencilActions);

    OutRTInfo.DepthStencilRenderTarget = RHIDepthRenderTargetView(DepthStencilRenderTarget.DepthStencilTarget,
                                                                  DepthLoadAction,
                                                                  GetStoreAction(DepthActions),
                                                                  StencilLoadAction,
                                                                  GetStoreAction(StencilActions),
                                                                  DepthStencilRenderTarget.ExclusiveDepthStencil);
    OutRTInfo.bClearDepth = (DepthLoadAction == RenderTargetLoadAction::EClear);
    OutRTInfo.bClearStencil = (StencilLoadAction == RenderTargetLoadAction::EClear);

    // OutRTInfo.ShadingRateTexture = ShadingRateTexture;
    // OutRTInfo.ShadingRateTextureCombiner = ShadingRateTextureCombiner;
    OutRTInfo.MultiViewCount = MultiViewCount;
}