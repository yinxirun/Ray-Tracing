#include "descriptor_sets.h"
#include "pipeline.h"
#include "rhi.h"
#include "device.h"
#include "context.h"
#include "platform.h"
#include "state.h"
#include "util.h"
#include "configuration.h"
#include "shader_resources.h"
#include "RHI/RHIGlobals.h"
#include "resources.h"

#include <vector>

enum class SingleThreadedPSOCreateMode
{
    None = 0,
    All = 1,
    Precompile = 2,
    NonPrecompiled = 3,
};

// Enable to force singlethreaded creation of PSOs. Only intended as a workaround for buggy drivers
// 0: (default) Allow Async precompile PSO creation.
// 1: force singlethreaded creation of all PSOs.
// 2: force singlethreaded creation of precompile PSOs only.
// 3: force singlethreaded creation of non-precompile PSOs only.
static int32 GVulkanPSOForceSingleThreaded = (int32)SingleThreadedPSOCreateMode::All;

/// 1: Force all created PSOs to be evicted immediately. Only for debugging
int32 CVarPipelineDebugForceEvictImmediately = 0;

// Pipeline LRU cache
// 0: disable LRU
// 1: Enable LRU
int32 CVarEnableLRU = 0;

void GetVulkanShaders(const BoundShaderStateInput &BSI, VulkanShader *OutShaders[ShaderStage::NumStages])
{
    memset(OutShaders, 0, ShaderStage::NumStages * sizeof(*OutShaders));

    OutShaders[ShaderStage::Vertex] = static_cast<VulkanVertexShader *>(BSI.VertexShaderRHI);

    if (BSI.PixelShaderRHI)
    {
        OutShaders[ShaderStage::Pixel] = static_cast<VulkanPixelShader *>(BSI.PixelShaderRHI);
    }

    if (BSI.GetGeometryShader())
    {
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
        OutShaders[ShaderStage::Geometry] = ResourceCast(BSI.GetGeometryShader());
#else
        ensure(0);
#endif
    }
}

VulkanPipeline::VulkanPipeline(Device *InDevice)
    : device(InDevice), Pipeline(VK_NULL_HANDLE), Layout(nullptr) {}

VulkanPipeline::~VulkanPipeline()
{
    if (Pipeline != VK_NULL_HANDLE)
    {
        device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::DeferredDeletionQueue2::EType::Pipeline, Pipeline);
        Pipeline = VK_NULL_HANDLE;
    }
    /* we do NOT own Layout !*/
}

VulkanComputePipeline::VulkanComputePipeline(Device *InDevice)
    : VulkanPipeline(InDevice), computeShader(nullptr) {}

VulkanComputePipeline::~VulkanComputePipeline()
{
    device->NotifyDeletedComputePipeline(this);

    if (computeShader)
    {
        computeShader->Release();
    }
}

// 686
Archive &operator<<(Archive &Ar, GfxPipelineDesc::BlendAttachment &Attachment)
{
    // Modify VERSION if serialization changes
    Ar << Attachment.bBlend;
    Ar << Attachment.ColorBlendOp;
    Ar << Attachment.SrcColorBlendFactor;
    Ar << Attachment.DstColorBlendFactor;
    Ar << Attachment.AlphaBlendOp;
    Ar << Attachment.SrcAlphaBlendFactor;
    Ar << Attachment.DstAlphaBlendFactor;
    Ar << Attachment.ColorWriteMask;
    return Ar;
}

void GfxPipelineDesc::BlendAttachment::ReadFrom(const VkPipelineColorBlendAttachmentState &InState)
{
    bBlend = InState.blendEnable != VK_FALSE;
    ColorBlendOp = (uint8)InState.colorBlendOp;
    SrcColorBlendFactor = (uint8)InState.srcColorBlendFactor;
    DstColorBlendFactor = (uint8)InState.dstColorBlendFactor;
    AlphaBlendOp = (uint8)InState.alphaBlendOp;
    SrcAlphaBlendFactor = (uint8)InState.srcAlphaBlendFactor;
    DstAlphaBlendFactor = (uint8)InState.dstAlphaBlendFactor;
    ColorWriteMask = (uint8)InState.colorWriteMask;
}

void GfxPipelineDesc::BlendAttachment::WriteInto(VkPipelineColorBlendAttachmentState &Out) const
{
    Out.blendEnable = bBlend ? VK_TRUE : VK_FALSE;
    Out.colorBlendOp = (VkBlendOp)ColorBlendOp;
    Out.srcColorBlendFactor = (VkBlendFactor)SrcColorBlendFactor;
    Out.dstColorBlendFactor = (VkBlendFactor)DstColorBlendFactor;
    Out.alphaBlendOp = (VkBlendOp)AlphaBlendOp;
    Out.srcAlphaBlendFactor = (VkBlendFactor)SrcAlphaBlendFactor;
    Out.dstAlphaBlendFactor = (VkBlendFactor)DstAlphaBlendFactor;
    Out.colorWriteMask = (VkColorComponentFlags)ColorWriteMask;
}

// 725
void DescriptorSetLayoutBinding::ReadFrom(const VkDescriptorSetLayoutBinding &InState)
{
    Binding = InState.binding;
    ensure(InState.descriptorCount == 1);
    DescriptorType = InState.descriptorType;
    StageFlags = InState.stageFlags;
}

void DescriptorSetLayoutBinding::WriteInto(VkDescriptorSetLayoutBinding &Out) const
{
    Out.binding = Binding;
    Out.descriptorType = (VkDescriptorType)DescriptorType;
    Out.stageFlags = StageFlags;
}

Archive &operator<<(Archive &Ar, DescriptorSetLayoutBinding &Binding)
{
    // Modify VERSION if serialization changes
    Ar << Binding.Binding;
    // Ar << Binding.DescriptorCount;
    Ar << Binding.DescriptorType;
    Ar << Binding.StageFlags;
    return Ar;
}

// 752
void GfxPipelineDesc::VertexBinding::ReadFrom(const VkVertexInputBindingDescription &InState)
{
    Binding = InState.binding;
    InputRate = (uint16)InState.inputRate;
    Stride = InState.stride;
}

void GfxPipelineDesc::VertexBinding::WriteInto(VkVertexInputBindingDescription &Out) const
{
    Out.binding = Binding;
    Out.inputRate = (VkVertexInputRate)InputRate;
    Out.stride = Stride;
}

Archive &operator<<(Archive &Ar, GfxPipelineDesc::VertexBinding &Binding)
{
    // Modify VERSION if serialization changes
    Ar << Binding.Stride;
    Ar << Binding.Binding;
    Ar << Binding.InputRate;
    return Ar;
}

// 775
void GfxPipelineDesc::VertexAttribute::ReadFrom(const VkVertexInputAttributeDescription &InState)
{
    Binding = InState.binding;
    Format = (uint32)InState.format;
    Location = InState.location;
    Offset = InState.offset;
}

void GfxPipelineDesc::VertexAttribute::WriteInto(VkVertexInputAttributeDescription &Out) const
{
    Out.binding = Binding;
    Out.format = (VkFormat)Format;
    Out.location = Location;
    Out.offset = Offset;
}

Archive &operator<<(Archive &Ar, GfxPipelineDesc::VertexAttribute &Attribute)
{
    // Modify VERSION if serialization changes
    Ar << Attribute.Location;
    Ar << Attribute.Binding;
    Ar << Attribute.Format;
    Ar << Attribute.Offset;
    return Ar;
}

// 801
void GfxPipelineDesc::Rasterizer::ReadFrom(const VkPipelineRasterizationStateCreateInfo &InState)
{
    PolygonMode = InState.polygonMode;
    CullMode = InState.cullMode;
    DepthBiasSlopeScale = InState.depthBiasSlopeFactor;
    DepthBiasConstantFactor = InState.depthBiasConstantFactor;
}

void GfxPipelineDesc::Rasterizer::WriteInto(VkPipelineRasterizationStateCreateInfo &Out) const
{
    Out.polygonMode = (VkPolygonMode)PolygonMode;
    Out.cullMode = (VkCullModeFlags)CullMode;
    Out.frontFace = VK_FRONT_FACE_CLOCKWISE;
    Out.depthClampEnable = VK_FALSE;
    Out.depthBiasEnable = DepthBiasConstantFactor != 0.0f ? VK_TRUE : VK_FALSE;
    Out.rasterizerDiscardEnable = VK_FALSE;
    Out.depthBiasSlopeFactor = DepthBiasSlopeScale;
    Out.depthBiasConstantFactor = DepthBiasConstantFactor;
}

Archive &operator<<(Archive &Ar, GfxPipelineDesc::Rasterizer &Rasterizer)
{
    // Modify VERSION if serialization changes
    Ar << Rasterizer.PolygonMode;
    Ar << Rasterizer.CullMode;
    Ar << Rasterizer.DepthBiasSlopeScale;
    Ar << Rasterizer.DepthBiasConstantFactor;
    return Ar;
}

// 831
void GfxPipelineDesc::FDepthStencil::ReadFrom(const VkPipelineDepthStencilStateCreateInfo &InState)
{
    DepthCompareOp = (uint8)InState.depthCompareOp;
    bDepthTestEnable = InState.depthTestEnable != VK_FALSE;
    bDepthWriteEnable = InState.depthWriteEnable != VK_FALSE;
    bDepthBoundsTestEnable = InState.depthBoundsTestEnable != VK_FALSE;
    bStencilTestEnable = InState.stencilTestEnable != VK_FALSE;
    FrontFailOp = (uint8)InState.front.failOp;
    FrontPassOp = (uint8)InState.front.passOp;
    FrontDepthFailOp = (uint8)InState.front.depthFailOp;
    FrontCompareOp = (uint8)InState.front.compareOp;
    FrontCompareMask = (uint8)InState.front.compareMask;
    FrontWriteMask = InState.front.writeMask;
    FrontReference = InState.front.reference;
    BackFailOp = (uint8)InState.back.failOp;
    BackPassOp = (uint8)InState.back.passOp;
    BackDepthFailOp = (uint8)InState.back.depthFailOp;
    BackCompareOp = (uint8)InState.back.compareOp;
    BackCompareMask = (uint8)InState.back.compareMask;
    BackWriteMask = InState.back.writeMask;
    BackReference = InState.back.reference;
}

void GfxPipelineDesc::FDepthStencil::WriteInto(VkPipelineDepthStencilStateCreateInfo &Out) const
{
    Out.depthCompareOp = (VkCompareOp)DepthCompareOp;
    Out.depthTestEnable = bDepthTestEnable;
    Out.depthWriteEnable = bDepthWriteEnable;
    Out.depthBoundsTestEnable = bDepthBoundsTestEnable;
    Out.stencilTestEnable = bStencilTestEnable;
    Out.front.failOp = (VkStencilOp)FrontFailOp;
    Out.front.passOp = (VkStencilOp)FrontPassOp;
    Out.front.depthFailOp = (VkStencilOp)FrontDepthFailOp;
    Out.front.compareOp = (VkCompareOp)FrontCompareOp;
    Out.front.compareMask = FrontCompareMask;
    Out.front.writeMask = FrontWriteMask;
    Out.front.reference = FrontReference;
    Out.back.failOp = (VkStencilOp)BackFailOp;
    Out.back.passOp = (VkStencilOp)BackPassOp;
    Out.back.depthFailOp = (VkStencilOp)BackDepthFailOp;
    Out.back.compareOp = (VkCompareOp)BackCompareOp;
    Out.back.writeMask = BackWriteMask;
    Out.back.compareMask = BackCompareMask;
    Out.back.reference = BackReference;
}

Archive &operator<<(Archive &Ar, GfxPipelineDesc::FDepthStencil &DepthStencil)
{
    // Modify VERSION if serialization changes
    Ar << DepthStencil.DepthCompareOp;
    Ar << DepthStencil.bDepthTestEnable;
    Ar << DepthStencil.bDepthWriteEnable;
    Ar << DepthStencil.bDepthBoundsTestEnable;
    Ar << DepthStencil.bStencilTestEnable;
    Ar << DepthStencil.FrontFailOp;
    Ar << DepthStencil.FrontPassOp;
    Ar << DepthStencil.FrontDepthFailOp;
    Ar << DepthStencil.FrontCompareOp;
    Ar << DepthStencil.FrontCompareMask;
    Ar << DepthStencil.FrontWriteMask;
    Ar << DepthStencil.FrontReference;
    Ar << DepthStencil.BackFailOp;
    Ar << DepthStencil.BackPassOp;
    Ar << DepthStencil.BackDepthFailOp;
    Ar << DepthStencil.BackCompareOp;
    Ar << DepthStencil.BackCompareMask;
    Ar << DepthStencil.BackWriteMask;
    Ar << DepthStencil.BackReference;
    return Ar;
}

// 902
void GfxPipelineDesc::FRenderTargets::FAttachmentRef::ReadFrom(const VkAttachmentReference &InState)
{
    Attachment = InState.attachment;
    Layout = (uint64)InState.layout;
}

void GfxPipelineDesc::FRenderTargets::FAttachmentRef::WriteInto(VkAttachmentReference &Out) const
{
    Out.attachment = Attachment;
    Out.layout = (VkImageLayout)Layout;
}

Archive &operator<<(Archive &Ar, GfxPipelineDesc::FRenderTargets::FAttachmentRef &AttachmentRef)
{
    // Modify VERSION if serialization changes
    Ar << AttachmentRef.Attachment;
    Ar << AttachmentRef.Layout;
    return Ar;
}

// 922
void GfxPipelineDesc::FRenderTargets::FStencilAttachmentRef::ReadFrom(const VkAttachmentReferenceStencilLayout &InState)
{
    Layout = (uint64)InState.stencilLayout;
}

void GfxPipelineDesc::FRenderTargets::FStencilAttachmentRef::WriteInto(VkAttachmentReferenceStencilLayout &Out) const
{
    Out.stencilLayout = (VkImageLayout)Layout;
}

Archive &operator<<(Archive &Ar, GfxPipelineDesc::FRenderTargets::FStencilAttachmentRef &AttachmentRef)
{
    // Modify VERSION if serialization changes
    Ar << AttachmentRef.Layout;
    return Ar;
}

// 939
void GfxPipelineDesc::FRenderTargets::FAttachmentDesc::ReadFrom(const VkAttachmentDescription &InState)
{
    Format = (uint32)InState.format;
    Flags = (uint8)InState.flags;
    Samples = (uint8)InState.samples;
    LoadOp = (uint8)InState.loadOp;
    StoreOp = (uint8)InState.storeOp;
    StencilLoadOp = (uint8)InState.stencilLoadOp;
    StencilStoreOp = (uint8)InState.stencilStoreOp;
    InitialLayout = (uint64)InState.initialLayout;
    FinalLayout = (uint64)InState.finalLayout;
}

void GfxPipelineDesc::FRenderTargets::FAttachmentDesc::WriteInto(VkAttachmentDescription &Out) const
{
    Out.format = (VkFormat)Format;
    Out.flags = Flags;
    Out.samples = (VkSampleCountFlagBits)Samples;
    Out.loadOp = (VkAttachmentLoadOp)LoadOp;
    Out.storeOp = (VkAttachmentStoreOp)StoreOp;
    Out.stencilLoadOp = (VkAttachmentLoadOp)StencilLoadOp;
    Out.stencilStoreOp = (VkAttachmentStoreOp)StencilStoreOp;
    Out.initialLayout = (VkImageLayout)InitialLayout;
    Out.finalLayout = (VkImageLayout)FinalLayout;
}

Archive &operator<<(Archive &Ar, GfxPipelineDesc::FRenderTargets::FAttachmentDesc &AttachmentDesc)
{
    // Modify VERSION if serialization changes
    Ar << AttachmentDesc.Format;
    Ar << AttachmentDesc.Flags;
    Ar << AttachmentDesc.Samples;
    Ar << AttachmentDesc.LoadOp;
    Ar << AttachmentDesc.StoreOp;
    Ar << AttachmentDesc.StencilLoadOp;
    Ar << AttachmentDesc.StencilStoreOp;
    Ar << AttachmentDesc.InitialLayout;
    Ar << AttachmentDesc.FinalLayout;
    return Ar;
}

// 981
void GfxPipelineDesc::FRenderTargets::StencilAttachmentDesc::ReadFrom(const VkAttachmentDescriptionStencilLayout &InState)
{
    InitialLayout = (uint64)InState.stencilInitialLayout;
    FinalLayout = (uint64)InState.stencilFinalLayout;
}

void GfxPipelineDesc::FRenderTargets::StencilAttachmentDesc::WriteInto(VkAttachmentDescriptionStencilLayout &Out) const
{
    Out.stencilInitialLayout = (VkImageLayout)InitialLayout;
    Out.stencilFinalLayout = (VkImageLayout)FinalLayout;
}

Archive &operator<<(Archive &Ar, GfxPipelineDesc::FRenderTargets::StencilAttachmentDesc &StencilAttachmentDesc)
{
    // Modify VERSION if serialization changes
    Ar << StencilAttachmentDesc.InitialLayout;
    Ar << StencilAttachmentDesc.FinalLayout;
    return Ar;
}

// 1002
void GfxPipelineDesc::FRenderTargets::ReadFrom(const RenderTargetLayout &RTLayout)
{
    NumAttachments = RTLayout.NumAttachmentDescriptions;
    NumColorAttachments = RTLayout.NumColorAttachments;

    bHasDepthStencil = RTLayout.bHasDepthStencil != 0;
    bHasResolveAttachments = RTLayout.bHasResolveAttachments != 0;
    bHasFragmentDensityAttachment = RTLayout.bHasFragmentDensityAttachment != 0;
    NumUsedClearValues = RTLayout.NumUsedClearValues;

    RenderPassCompatibleHash = RTLayout.GetRenderPassCompatibleHash();

    Extent3D.x = RTLayout.Extent.Extent3D.width;
    Extent3D.y = RTLayout.Extent.Extent3D.height;
    Extent3D.z = RTLayout.Extent.Extent3D.depth;

    auto CopyAttachmentRefs = [&](std::vector<GfxPipelineDesc::FRenderTargets::FAttachmentRef> &Dest, const VkAttachmentReference *Source, uint32 Count)
    {
        for (uint32 Index = 0; Index < Count; ++Index)
        {
            Dest.push_back({});
            GfxPipelineDesc::FRenderTargets::FAttachmentRef &New = Dest.back();
            New.ReadFrom(Source[Index]);
        }
    };
    CopyAttachmentRefs(ColorAttachments, RTLayout.ColorReferences, MaxSimultaneousRenderTargets);
    CopyAttachmentRefs(ResolveAttachments, RTLayout.ResolveReferences, MaxSimultaneousRenderTargets);
    Depth.ReadFrom(RTLayout.DepthReference);
    Stencil.ReadFrom(RTLayout.StencilReference);
    FragmentDensity.ReadFrom(RTLayout.FragmentDensityReference);

    Descriptions.resize(MaxSimultaneousRenderTargets * 2 + 2);
    for (int32 Index = 0; Index < MaxSimultaneousRenderTargets * 2 + 2; ++Index)
    {
        Descriptions[Index].ReadFrom(RTLayout.Desc[Index]);
    }
    StencilDescription.ReadFrom(RTLayout.StencilDesc);
}

void GfxPipelineDesc::FRenderTargets::WriteInto(RenderTargetLayout &Out) const
{
    Out.NumAttachmentDescriptions = NumAttachments;
    Out.NumColorAttachments = NumColorAttachments;

    Out.bHasDepthStencil = bHasDepthStencil;
    Out.bHasResolveAttachments = bHasResolveAttachments;
    Out.bHasFragmentDensityAttachment = bHasFragmentDensityAttachment;
    Out.NumUsedClearValues = NumUsedClearValues;

    ensure(0);
    Out.RenderPassCompatibleHash = RenderPassCompatibleHash;

    Out.Extent.Extent3D.width = Extent3D.x;
    Out.Extent.Extent3D.height = Extent3D.y;
    Out.Extent.Extent3D.depth = Extent3D.z;

    auto CopyAttachmentRefs = [&](const std::vector<GfxPipelineDesc::FRenderTargets::FAttachmentRef> &Source, VkAttachmentReference *Dest, uint32 Count)
    {
        for (uint32 Index = 0; Index < Count; ++Index, ++Dest)
        {
            Source[Index].WriteInto(*Dest);
        }
    };
    CopyAttachmentRefs(ColorAttachments, Out.ColorReferences, MaxSimultaneousRenderTargets);
    CopyAttachmentRefs(ResolveAttachments, Out.ResolveReferences, MaxSimultaneousRenderTargets);
    Depth.WriteInto(Out.DepthReference);
    Stencil.WriteInto(Out.StencilReference);
    FragmentDensity.WriteInto(Out.FragmentDensityReference);

    for (int32 Index = 0; Index < MaxSimultaneousRenderTargets * 2 + 2; ++Index)
    {
        Descriptions[Index].WriteInto(Out.Desc[Index]);
    }
    StencilDescription.WriteInto(Out.StencilDesc);
}

Archive &operator<<(Archive &Ar, GfxPipelineDesc::FRenderTargets &RTs)
{
    // Modify VERSION if serialization changes
    Ar << RTs.NumAttachments;
    Ar << RTs.NumColorAttachments;
    Ar << RTs.NumUsedClearValues;
    Ar << RTs.ColorAttachments;
    Ar << RTs.ResolveAttachments;
    Ar << RTs.Depth;
    Ar << RTs.Stencil;
    Ar << RTs.FragmentDensity;

    Ar << RTs.Descriptions;
    Ar << RTs.StencilDescription;

    Ar << RTs.bHasDepthStencil;
    Ar << RTs.bHasResolveAttachments;
    Ar << RTs.RenderPassCompatibleHash;
    Ar << RTs.Extent3D;

    return Ar;
}

Archive &operator<<(Archive &Ar, GfxPipelineDesc &Entry)
{
    // Modify VERSION if serialization changes
    Ar << Entry.VertexInputKey;
    Ar << Entry.RasterizationSamples;
    Ar << Entry.ControlPoints;
    Ar << Entry.Topology;

    Ar << Entry.ColorAttachmentStates;
    Ar << Entry.ColorAttachmentStates[0];

    Ar << Entry.DescriptorSetLayoutBindings;

    Ar << Entry.VertexBindings;
    Ar << Entry.VertexAttributes;
    Ar << Entry.rasterizer;

    Ar << Entry.DepthStencil;

#if VULKAN_USE_SHADERKEYS
    for (uint64 &ShaderKey : Entry.ShaderKeys)
    {
        Ar << ShaderKey;
    }
#else
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Entry.ShaderHashes.Stages); ++Index)
    {
        Ar << Entry.ShaderHashes.Stages[Index];
    }
#endif
    Ar << Entry.RenderTargets;

    uint8 ShadingRate = static_cast<uint8>(Entry.ShadingRate);
    uint8 Combiner = static_cast<uint8>(Entry.Combiner);

    Ar << ShadingRate;
    Ar << Combiner;

    Ar << Entry.UseAlphaToCoverage;

    return Ar;
}

VulkanPSOKey GfxPipelineDesc::CreateKey2() const
{
    VulkanPSOKey Result;
    Result.GenerateFromArchive([this](Archive &Ar)
                               { Ar << const_cast<GfxPipelineDesc &>(*this); });
    return Result;
}

VulkanGraphicsPipelineState::VulkanGraphicsPipelineState(Device *device, const GraphicsPipelineStateInitializer &PSOInitializer_,
                                                         GfxPipelineDesc &Desc, VulkanPSOKey *VulkanKey)
    : bIsRegistered(false), PrimitiveType(PSOInitializer_.PrimitiveType),
      VulkanPipeline(0), device(device), Desc(Desc), VulkanKey(VulkanKey->CopyDeep())
{
    memset(VulkanShaders, 0, sizeof(VulkanShaders));
    VulkanShaders[ShaderStage::Vertex] = static_cast<VulkanVertexShader *>(PSOInitializer_.BoundShaderState.VertexShaderRHI);
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    VulkanShaders[ShaderStage::Geometry] = static_cast<FVulkanGeometryShader *>(PSOInitializer_.BoundShaderState.GetGeometryShader());
#endif
    VulkanShaders[ShaderStage::Pixel] = static_cast<VulkanPixelShader *>(PSOInitializer_.BoundShaderState.PixelShaderRHI);

    /*for (int ShaderStageIndex = 0; ShaderStageIndex < ShaderStage::NumStages; ShaderStageIndex++)
    {
        if (VulkanShaders[ShaderStageIndex] != nullptr)
        {
            VulkanShaders[ShaderStageIndex]->AddRef();
        }
    }*/

    PrecacheKey = RHI::Get().ComputePrecachePSOHash(PSOInitializer_);
}

VulkanGraphicsPipelineState::~VulkanGraphicsPipelineState()
{
    // 所有shader由shader factory负责回收
    device->PipelineStateCache->NotifyDeletedGraphicsPSO(this);
}

void PipelineStateCacheManager::InitAndLoad(const std::vector<std::string> &CacheFilenames)
{
    for (const std::string &s : CacheFilenames)
    {
        printf("Cache Filename: %s\n", s.c_str());
    }
}

PipelineStateCacheManager::PipelineStateCacheManager(Device *InDevice)
    : device(InDevice), bEvictImmediately(false), bPrecompilingCacheLoadedFromFile(false)
{
    bUseLRU = 0;
    LRUUsedPipelineMax = 0;
}

PipelineStateCacheManager::~PipelineStateCacheManager()
{
    DestroyCache();

    // Only destroy layouts when quitting
    for (auto &Pair : LayoutMap)
    {
        delete Pair.second;
    }
    for (auto &Pair : DSetLayoutMap)
    {
        vkDestroyDescriptorSetLayout(device->GetInstanceHandle(), Pair.second.Handle, VULKAN_CPU_ALLOCATOR);
    }
}

static inline VkPrimitiveTopology UEToVulkanTopologyType(const Device *InDevice, PrimitiveType PrimitiveType, uint16 &OutControlPoints)
{
    OutControlPoints = 0;
    switch (PrimitiveType)
    {
    case PT_PointList:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case PT_LineList:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case PT_TriangleList:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case PT_TriangleStrip:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    default:
        check(false);
        break;
    }

    return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

void PipelineStateCacheManager::CreateGfxEntry(const GraphicsPipelineStateInitializer &PSOInitializer,
                                               DescriptorSetsLayoutInfo &DescriptorSetLayoutInfo, GfxPipelineDesc *Desc)
{
    GfxPipelineDesc *OutGfxEntry = Desc;

    VulkanShader *Shaders[ShaderStage::NumStages];
    GetVulkanShaders(PSOInitializer.BoundShaderState, Shaders);

    VulkanVertexInputStateInfo VertexInputState;
    {
        const BoundShaderStateInput &BSI = PSOInitializer.BoundShaderState;

        const ShaderHeader &VSHeader = Shaders[ShaderStage::Vertex]->GetCodeHeader();
        VertexInputState.Generate(static_cast<VulkanVertexDeclaration *>(PSOInitializer.BoundShaderState.VertexDeclarationRHI), VSHeader.InOutMask);

        UniformBufferGatherInfo UBGatherInfo;

        DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_VERTEX_BIT, ShaderStage::Vertex, VSHeader, UBGatherInfo);

        const ShaderHeader &PSHeader = Shaders[ShaderStage::Pixel]->GetCodeHeader();
        DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_FRAGMENT_BIT, ShaderStage::Pixel, PSHeader, UBGatherInfo);

#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
        if (Shaders[ShaderStage::Geometry])
        {
            const FVulkanShaderHeader &GSHeader = Shaders[ShaderStage::Geometry]->GetCodeHeader();
            DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_GEOMETRY_BIT, ShaderStage::Geometry, GSHeader, UBGatherInfo);
        }
#endif

        // Second pass
        const int32 NumImmutableSamplers = PSOInitializer.ImmutableSamplerState.ImmutableSamplers.size();
        std::vector<SamplerState *> ImmutableSamplers(NumImmutableSamplers, nullptr);
        for (int i = 0; i < NumImmutableSamplers; ++i)
        {
            ImmutableSamplers[i] = PSOInitializer.ImmutableSamplerState.ImmutableSamplers[i];
        }

        DescriptorSetLayoutInfo.FinalizeBindings<false>(*device, UBGatherInfo, ImmutableSamplers);
    }

    DescriptorSetRemappingInfo &RemappingInfo = DescriptorSetLayoutInfo.RemappingInfo;

    if (RemappingInfo.inputAttachmentData.size())
    {
        // input attachements can't exist in a first sub-pass
        check(PSOInitializer.SubpassHint != SubpassHint::None);
        check(PSOInitializer.SubpassIndex != 0);
    }

    OutGfxEntry->SubpassIndex = PSOInitializer.SubpassIndex;

    VulkanBlendState *BlendState = static_cast<VulkanBlendState *>(PSOInitializer.BlendState);

    OutGfxEntry->UseAlphaToCoverage = PSOInitializer.NumSamples > 1 && BlendState->Initializer.bUseAlphaToCoverage ? 1 : 0;

    OutGfxEntry->RasterizationSamples = PSOInitializer.NumSamples;
    OutGfxEntry->Topology = (uint32)UEToVulkanTopologyType(device, PSOInitializer.PrimitiveType, OutGfxEntry->ControlPoints);
    uint32 NumRenderTargets = PSOInitializer.ComputeNumValidRenderTargets();

    if (PSOInitializer.SubpassHint == SubpassHint::DeferredShadingSubpass && PSOInitializer.SubpassIndex >= 2)
    {
        // GBuffer attachements are not used as output in a shading sub-pass
        // Only SceneColor is used as a color attachment
        NumRenderTargets = 1;
    }

    if (PSOInitializer.SubpassHint == SubpassHint::DepthReadSubpass && PSOInitializer.SubpassIndex >= 1)
    {
        // Only SceneColor is used as a color attachment after the first subpass (not SceneDepthAux)
        NumRenderTargets = 1;
    }

    if (PSOInitializer.SubpassHint == SubpassHint::CustomResolveSubpass)
    {
        NumRenderTargets = 1; // This applies to base and depth passes as well. One render target for base and depth, another one for custom resolve.
        if (PSOInitializer.SubpassIndex >= 2)
        {
            // the resolve subpass renders to a non MSAA surface
            OutGfxEntry->RasterizationSamples = 1;
        }
    }

    OutGfxEntry->ColorAttachmentStates.resize(NumRenderTargets);
    for (int32 Index = 0; Index < OutGfxEntry->ColorAttachmentStates.size(); ++Index)
    {
        OutGfxEntry->ColorAttachmentStates[Index].ReadFrom(BlendState->BlendStates[Index]);
    }

    {
        const VkPipelineVertexInputStateCreateInfo &VBInfo = VertexInputState.GetInfo();
        OutGfxEntry->VertexBindings.resize(VBInfo.vertexBindingDescriptionCount);
        for (uint32 Index = 0; Index < VBInfo.vertexBindingDescriptionCount; ++Index)
        {
            OutGfxEntry->VertexBindings[Index].ReadFrom(VBInfo.pVertexBindingDescriptions[Index]);
        }

        OutGfxEntry->VertexAttributes.resize(VBInfo.vertexAttributeDescriptionCount);
        for (uint32 Index = 0; Index < VBInfo.vertexAttributeDescriptionCount; ++Index)
        {
            OutGfxEntry->VertexAttributes[Index].ReadFrom(VBInfo.pVertexAttributeDescriptions[Index]);
        }
    }

    const std::vector<DescriptorSetsLayout::SetLayout> &Layouts = DescriptorSetLayoutInfo.GetLayouts();
    OutGfxEntry->DescriptorSetLayoutBindings.resize(Layouts.size());
    for (int32 Index = 0; Index < Layouts.size(); ++Index)
    {
        for (int32 SubIndex = 0; SubIndex < Layouts[Index].LayoutBindings.size(); ++SubIndex)
        {
            OutGfxEntry->DescriptorSetLayoutBindings[Index].push_back({});
            DescriptorSetLayoutBinding &Binding = OutGfxEntry->DescriptorSetLayoutBindings[Index].back();
            Binding.ReadFrom(Layouts[Index].LayoutBindings[SubIndex]);
        }
    }

    OutGfxEntry->rasterizer.ReadFrom(static_cast<VulkanRasterizerState *>(PSOInitializer.RasterizerState)->RasterizerState);

    {
        VkPipelineDepthStencilStateCreateInfo DSInfo;
        static_cast<VulkanDepthStencilState *>(PSOInitializer.DepthStencilState)->SetupCreateInfo(PSOInitializer, DSInfo);
        OutGfxEntry->DepthStencil.ReadFrom(DSInfo);
    }

    int32 NumShaders = 0;
#if VULKAN_USE_SHADERKEYS
    uint64 SharedKey = 0;
    uint64 Primes[] = {
        6843488303525203279llu,
        3095754086865563867llu,
        8242695776924673527llu,
        7556751872809527943llu,
        8278265491465149053llu,
        1263027877466626099llu,
        2698115308251696101llu,
    };
    static_assert(sizeof(Primes) / sizeof(Primes[0]) >= ShaderStage::NumStages);
    for (int32 Index = 0; Index < ShaderStage::NumStages; ++Index)
    {
        VulkanShader *Shader = Shaders[Index];
        uint64 Key = 0;
        if (Shader)
        {
            Key = Shader->GetShaderKey();
            ++NumShaders;
        }
        OutGfxEntry->ShaderKeys[Index] = Key;
        SharedKey += Key * Primes[Index];
    }
    OutGfxEntry->ShaderKeyShared = SharedKey;
#else
    for (int32 Index = 0; Index < ShaderStage::NumStages; ++Index)
    {
        VulkanShader *Shader = Shaders[Index];
        if (Shader)
        {
            check(Shader->Spirv.size() != 0);

            SHAHash Hash = GetShaderHashForStage(PSOInitializer, (ShaderStage::EStage)Index);
            OutGfxEntry->ShaderHashes.Stages[Index] = Hash;

            ++NumShaders;
        }
    }
    OutGfxEntry->ShaderHashes.Finalize();
#endif
    check(NumShaders > 0);

    RenderTargetLayout RTLayout(PSOInitializer);
    OutGfxEntry->RenderTargets.ReadFrom(RTLayout);

    // Shading rate:
    OutGfxEntry->ShadingRate = PSOInitializer.ShadingRate;
    OutGfxEntry->Combiner = VRSRateCombiner::VRSRB_Max; // @todo: This needs to be specified twice; from pipeline-to-primitive, and from primitive-to-attachment.
                                                        // We don't have per-primitive VRS so that should just be hard-coded to "passthrough" until this is supported; but we should expose
                                                        // this setting in the material properies, especially since there's some materials that don't play nicely with
                                                        // shading rates other than 1x1, in which case we'll want to use VRSRB_Min to override e.g. the attachment shading rate.
                                                        // For now, just locked to "max".
}

// 1658
void PipelineStateCacheManager::DestroyCache()
{
    VkDevice DeviceHandle = device->GetInstanceHandle();

    /* FScopeLock Lock1(&GraphicsPSOLockedCS); */
    int idx = 0;
    for (auto &Pair : GraphicsPSOLockedMap)
    {
        VulkanGraphicsPipelineState *Pipeline = Pair.second;
        printf("Leaked PSO Handle %d: RefCount=%d\n", Pipeline, Pipeline->GetRefCount());
    }
    /* LRU2SizeList.Reset(); */

    // Compute pipelines already deleted...
    /* ComputePipelineEntries.Reset(); */
}

// 1705
VulkanPipelineLayout *PipelineStateCacheManager::FindOrAddLayout(const DescriptorSetsLayoutInfo &DescriptorSetLayoutInfo, bool bGfxLayout)
{
    /* FScopeLock Lock(&LayoutMapCS); */

    auto it = LayoutMap.find(DescriptorSetLayoutInfo);
    if (it != LayoutMap.end())
    {
        check(bGfxLayout == it->second->IsGfxLayout());
        return it->second;
    }

    VulkanPipelineLayout *Layout = nullptr;
    VulkanGfxLayout *GfxLayout = nullptr;

    if (bGfxLayout)
    {
        GfxLayout = new VulkanGfxLayout(device);
        Layout = GfxLayout;
    }
    else
    {
        Layout = new VulkanComputeLayout(device);
    }

    Layout->descriptorSetsLayout.CopyFrom(DescriptorSetLayoutInfo);
    Layout->Compile(DSetLayoutMap);

    if (GfxLayout)
    {
        GfxLayout->GfxPipelineDescriptorInfo.Initialize(GfxLayout->GetDescriptorSetsLayout().RemappingInfo);
    }

    LayoutMap.insert(std::pair(DescriptorSetLayoutInfo, Layout));
    return Layout;
}

// 1983
void PipelineStateCacheManager::NotifyDeletedGraphicsPSO(GraphicsPipelineState *PSO)
{
    VulkanGraphicsPipelineState *VkPSO = (VulkanGraphicsPipelineState *)PSO;
    device->NotifyDeletedGfxPipeline(VkPSO);
    VulkanPSOKey &Key = VkPSO->VulkanKey;

    if (VkPSO->bIsRegistered)
    {
        /* FScopeLock Lock(&GraphicsPSOLockedCS); */
        auto it = GraphicsPSOLockedMap.find(Key);
        VulkanGraphicsPipelineState **Contained = it == GraphicsPSOLockedMap.end() ? nullptr : &it->second;
        check(Contained && *Contained == PSO);
        VkPSO->bIsRegistered = false;
        if (bUseLRU)
        {
            check(0);
        }
        else
        {
            (*Contained)->DeleteVkPipeline(true);
            check(VkPSO->GetVulkanPipeline() == 0);
        }
        GraphicsPSOLockedMap.erase(Key);
    }
    else
    {
        /* FScopeLock Lock(&GraphicsPSOLockedCS); */
        auto it = GraphicsPSOLockedMap.find(Key);
        if (it != GraphicsPSOLockedMap.end())
        {
            check(0);
        }
        VkPSO->DeleteVkPipeline(true);
    }
}

// 2042
GraphicsPipelineState *PipelineStateCacheManager::CreateGraphicsPipelineState(const GraphicsPipelineStateInitializer &Initializer)
{
    // Optional lock for PSO creation, GVulkanPSOForceSingleThreaded is used to work around driver bugs.
    // GVulkanPSOForceSingleThreaded == Precompile can be used when the driver internally serializes PSO creation, this option reduces the driver queue size.
    // We stall precompile PSOs which increases the likelihood for non-precompile PSO to jump the queue.
    // Not using GraphicsPSOLockedCS as the create could take a long time on some platforms, holding GraphicsPSOLockedCS the whole time could cause hitching.
    const SingleThreadedPSOCreateMode ThreadingMode = (SingleThreadedPSOCreateMode)GVulkanPSOForceSingleThreaded;
    const bool bIsPrecache = Initializer.bFromPSOFileCache || Initializer.bPSOPrecache;
    bool bShouldLock = ThreadingMode == SingleThreadedPSOCreateMode::All ||
                       (ThreadingMode == SingleThreadedPSOCreateMode::Precompile && bIsPrecache) ||
                       (ThreadingMode == SingleThreadedPSOCreateMode::NonPrecompiled && !bIsPrecache);

    // FPSOOptionalLock PSOSingleThreadedLock(bShouldLock ? &CreateGraphicsPSOMutex : nullptr);

    VulkanPSOKey Key;
    GfxPipelineDesc Desc;
    DescriptorSetsLayoutInfo DescriptorSetLayoutInfo;
    {
        CreateGfxEntry(Initializer, DescriptorSetLayoutInfo, &Desc);
        Key = Desc.CreateKey2();
    }

    VulkanGraphicsPipelineState *NewPSO = 0;
    {
        {
            auto it = GraphicsPSOLockedMap.find(Key);
            if (it != GraphicsPSOLockedMap.end())
            {
                VulkanGraphicsPipelineState **PSO = &it->second;
                check(*PSO);
                if (!bIsPrecache)
                {
                    LRUTouch(*PSO);
                }
                return *PSO;
            }
        }
    }

    {
        // Workers can be creating PSOs while FRHIResource::FlushPendingDeletes is running on the RHI thread
        // so let it get enqueued for a delete with Release() instead.  Only used for failed or duplicate PSOs...
        auto DeleteNewPSO = [](VulkanGraphicsPipelineState *PSOPtr)
        {
            PSOPtr->AddRef();
            const uint32 RefCount = PSOPtr->Release();
            check(RefCount == 0);
        };

        NewPSO = new VulkanGraphicsPipelineState(device, Initializer, Desc, &Key);
        {
            VulkanPipelineLayout *Layout = FindOrAddLayout(DescriptorSetLayoutInfo, true);
            VulkanGfxLayout *GfxLayout = (VulkanGfxLayout *)Layout;
            check(GfxLayout->GfxPipelineDescriptorInfo.IsInitialized());
            NewPSO->Layout = GfxLayout;
            NewPSO->bHasInputAttachments = GfxLayout->GetDescriptorSetsLayout().HasInputAttachments();
        }
        NewPSO->RenderPass = device->GetImmediateContext().PrepareRenderPassForPSOCreation(Initializer);
        {
            const BoundShaderStateInput &BSI = Initializer.BoundShaderState;
            for (int32 StageIdx = 0; StageIdx < ShaderStage::NumStages; ++StageIdx)
            {
                NewPSO->ShaderKeys[StageIdx] = GetShaderKeyForGfxStage(BSI, (ShaderStage::Stage)StageIdx);
            }

            check(BSI.VertexShaderRHI);
            VulkanVertexShader *VS = static_cast<VulkanVertexShader *>(BSI.VertexShaderRHI);
            const ShaderHeader &VSHeader = VS->GetCodeHeader();
            NewPSO->VertexInputState.Generate(static_cast<VulkanVertexDeclaration *>(Initializer.BoundShaderState.VertexDeclarationRHI), VSHeader.InOutMask);

            if ((!bIsPrecache || !LRUEvictImmediately()) && 0 == CVarPipelineDebugForceEvictImmediately)
            {
                // Create the pipeline
                VulkanShader *VulkanShaders[ShaderStage::NumStages];
                GetVulkanShaders(Initializer.BoundShaderState, VulkanShaders);
                for (int32 StageIdx = 0; StageIdx < ShaderStage::NumStages; ++StageIdx)
                {
                    uint64 key = GetShaderKeyForGfxStage(BSI, (ShaderStage::Stage)StageIdx);
                    check(key == NewPSO->ShaderKeys[StageIdx]);
                }

                if (!CreateGfxPipelineFromEntry(NewPSO, VulkanShaders, bIsPrecache))
                {
                    DeleteNewPSO(NewPSO);
                    return nullptr;
                }
            }
            /* FScopeLock Lock(&GraphicsPSOLockedCS); */
            auto it = GraphicsPSOLockedMap.find(Key);
            if (it != GraphicsPSOLockedMap.end()) // another thread could end up creating it.
            {
                VulkanGraphicsPipelineState **MapPSO = &it->second;
                DeleteNewPSO(NewPSO);
                NewPSO = *MapPSO;
            }
            else
            {
                GraphicsPSOLockedMap.insert(std::pair(std::move(Key), NewPSO));
                if (bUseLRU && NewPSO->VulkanPipeline != VK_NULL_HANDLE)
                {
                    // we add only created pipelines to the LRU
                    /* FScopeLock LockRU(&LRUCS); */
                    NewPSO->bIsRegistered = true;
                    LRUTrim(NewPSO->PipelineCacheSize);
                    LRUAdd(NewPSO);
                }
                else
                {
                    NewPSO->bIsRegistered = true;
                }
            }
        }
    }
    return NewPSO;
}

bool PipelineStateCacheManager::CreateGfxPipelineFromEntry(VulkanGraphicsPipelineState *PSO,
                                                           VulkanShader *Shaders[ShaderStage::NumStages],
                                                           bool bPrecompile)
{
    VkPipeline *Pipeline = &PSO->VulkanPipeline;
    GfxPipelineDesc *GfxEntry = &PSO->Desc;
    if (Shaders[ShaderStage::Pixel] == nullptr && !Platform::SupportsNullPixelShader())
    {
        printf("ERROR: Reject to process this problem! %s %d\n", __FILE__, __LINE__);
        exit(-1);
    }

    std::shared_ptr<ShaderModule> ShaderModules[ShaderStage::NumStages];

    PSO->GetOrCreateShaderModules(ShaderModules, Shaders);

    // Pipeline
    VkGraphicsPipelineCreateInfo PipelineInfo;
    ZeroVulkanStruct(PipelineInfo, VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
    PipelineInfo.layout = PSO->Layout->GetPipelineLayout();

    // Color Blend
    VkPipelineColorBlendStateCreateInfo CBInfo;
    ZeroVulkanStruct(CBInfo, VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
    CBInfo.attachmentCount = GfxEntry->ColorAttachmentStates.size();
    VkPipelineColorBlendAttachmentState BlendStates[MaxSimultaneousRenderTargets];
    memset(&BlendStates, 0, sizeof(VkPipelineColorBlendAttachmentState));
    uint32 ColorWriteMask = 0xffffffff;
    if (Shaders[ShaderStage::Pixel])
    {
        ColorWriteMask = Shaders[ShaderStage::Pixel]->CodeHeader.InOutMask;
    }
    for (int32 Index = 0; Index < GfxEntry->ColorAttachmentStates.size(); ++Index)
    {
        GfxEntry->ColorAttachmentStates[Index].WriteInto(BlendStates[Index]);

        if (0 == (ColorWriteMask & 1)) // clear write mask of rendertargets not written by pixelshader.
        {
            BlendStates[Index].colorWriteMask = 0;
        }
        ColorWriteMask >>= 1;
    }
    CBInfo.pAttachments = BlendStates;
    CBInfo.blendConstants[0] = 1.0f;
    CBInfo.blendConstants[1] = 1.0f;
    CBInfo.blendConstants[2] = 1.0f;
    CBInfo.blendConstants[3] = 1.0f;

    // Viewport
    VkPipelineViewportStateCreateInfo VPInfo;
    ZeroVulkanStruct(VPInfo, VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
    VPInfo.viewportCount = 1;
    VPInfo.scissorCount = 1;

    // Multisample
    VkPipelineMultisampleStateCreateInfo MSInfo;
    ZeroVulkanStruct(MSInfo, VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
    MSInfo.rasterizationSamples = (VkSampleCountFlagBits)std::max<uint16>(1u, GfxEntry->RasterizationSamples);
    MSInfo.alphaToCoverageEnable = GfxEntry->UseAlphaToCoverage;

    VkPipelineShaderStageCreateInfo ShaderStages[ShaderStage::NumStages];
    memset(ShaderStages, 0, sizeof(VkPipelineShaderStageCreateInfo) * ShaderStage::NumStages);
    PipelineInfo.stageCount = 0;
    PipelineInfo.pStages = ShaderStages;

    char EntryPoints[ShaderStage::NumStages][24];
    VkPipelineShaderStageRequiredSubgroupSizeCreateInfo RequiredSubgroupSizeCreateInfo[ShaderStage::NumStages];
    for (int32 ShaderStage = 0; ShaderStage < ShaderStage::NumStages; ++ShaderStage)
    {
        if (!ShaderModules[ShaderStage]->IsValid() || (Shaders[ShaderStage] == nullptr))
        {
            continue;
        }
        const ShaderStage::Stage CurrStage = (ShaderStage::Stage)ShaderStage;

        ShaderStages[PipelineInfo.stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        const VkShaderStageFlagBits Stage = UEFrequencyToVKStageBit(ShaderStage::GetFrequencyForGfxStage(CurrStage));
        ShaderStages[PipelineInfo.stageCount].stage = Stage;
        ShaderStages[PipelineInfo.stageCount].module = ShaderModules[CurrStage]->GetVkShaderModule();
        Shaders[ShaderStage]->GetEntryPoint(EntryPoints[PipelineInfo.stageCount], 24);
        ShaderStages[PipelineInfo.stageCount].pName = EntryPoints[PipelineInfo.stageCount];

        if (device->GetOptionalExtensions().HasEXTSubgroupSizeControl)
        {
            printf("ERROR: Don't support Subgroup Size Control %s\n", __FILE__);
            exit(-1);
        }

        PipelineInfo.stageCount++;
    }

    check(PipelineInfo.stageCount != 0);

    // Vertex Input. The structure is mandatory even without vertex attributes.
    VkPipelineVertexInputStateCreateInfo VBInfo;
    ZeroVulkanStruct(VBInfo, VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
    std::vector<VkVertexInputBindingDescription> VBBindings;
    for (const GfxPipelineDesc::VertexBinding &SourceBinding : GfxEntry->VertexBindings)
    {
        VBBindings.push_back(VkVertexInputBindingDescription{});
        VkVertexInputBindingDescription &Binding = VBBindings.back();
        SourceBinding.WriteInto(Binding);
    }
    VBInfo.vertexBindingDescriptionCount = VBBindings.size();
    VBInfo.pVertexBindingDescriptions = VBBindings.data();
    std::vector<VkVertexInputAttributeDescription> VBAttributes;
    for (const GfxPipelineDesc::VertexAttribute &SourceAttr : GfxEntry->VertexAttributes)
    {
        VBAttributes.push_back(VkVertexInputAttributeDescription{});
        VkVertexInputAttributeDescription &Attr = VBAttributes.back();
        SourceAttr.WriteInto(Attr);
    }
    VBInfo.vertexAttributeDescriptionCount = VBAttributes.size();
    VBInfo.pVertexAttributeDescriptions = VBAttributes.data();
    PipelineInfo.pVertexInputState = &VBInfo;

    PipelineInfo.pColorBlendState = &CBInfo;
    PipelineInfo.pMultisampleState = &MSInfo;
    PipelineInfo.pViewportState = &VPInfo;

    PipelineInfo.renderPass = PSO->RenderPass->GetHandle();
    PipelineInfo.subpass = GfxEntry->SubpassIndex;

    VkPipelineInputAssemblyStateCreateInfo InputAssembly;
    ZeroVulkanStruct(InputAssembly, VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
    InputAssembly.topology = (VkPrimitiveTopology)GfxEntry->Topology;

    PipelineInfo.pInputAssemblyState = &InputAssembly;

    VkPipelineRasterizationStateCreateInfo RasterizerState;
    VulkanRasterizerState::ResetCreateInfo(RasterizerState);
    GfxEntry->rasterizer.WriteInto(RasterizerState);

    VkPipelineDepthStencilStateCreateInfo DepthStencilState;
    ZeroVulkanStruct(DepthStencilState, VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
    GfxEntry->DepthStencil.WriteInto(DepthStencilState);

    PipelineInfo.pRasterizationState = &RasterizerState;
    PipelineInfo.pDepthStencilState = &DepthStencilState;

    VkPipelineDynamicStateCreateInfo DynamicState;
    ZeroVulkanStruct(DynamicState, VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
    VkDynamicState DynamicStatesEnabled[4];
    DynamicState.pDynamicStates = DynamicStatesEnabled;
    memset(DynamicStatesEnabled, 0, sizeof(VkDynamicState) * 4);
    DynamicStatesEnabled[DynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
    DynamicStatesEnabled[DynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;
    DynamicStatesEnabled[DynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
    DynamicStatesEnabled[DynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;

    PipelineInfo.pDynamicState = &DynamicState;

    VkPipelineFragmentShadingRateStateCreateInfoKHR PipelineFragmentShadingRate;
    if (GRHISupportsPipelineVariableRateShading && GRHIVariableRateShadingEnabled && GRHIVariableRateShadingImageDataType == VRSImage_Palette)
    {
        printf("ERROR: Don't support VSR %s\n", __FILE__);
        exit(-1);
        // 	const VkExtent2D FragmentSize = Device->GetBestMatchedFragmentSize(PSO->Desc.ShadingRate);
        // 	VkFragmentShadingRateCombinerOpKHR PipelineToPrimitiveCombinerOperation = FragmentCombinerOpMap[(uint8)PSO->Desc.Combiner];

        // 	ZeroVulkanStruct(PipelineFragmentShadingRate, VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR);
        // 	PipelineFragmentShadingRate.fragmentSize = FragmentSize;
        // 	PipelineFragmentShadingRate.combinerOps[0] = PipelineToPrimitiveCombinerOperation;
        // 	PipelineFragmentShadingRate.combinerOps[1] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR;		// @todo: This needs to be specified too.

        // 	PipelineInfo.pNext = (void*)&PipelineFragmentShadingRate;
    }

    if (device->SupportsBindless())
    {
        PipelineInfo.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    }

    VkResult Result = VK_ERROR_INITIALIZATION_FAILED;

    // double BeginTime = FPlatformTime::Seconds();

    Result = CreateVKPipeline(PSO, Shaders, PipelineInfo, bPrecompile);

    if (Result != VK_SUCCESS)
    {
        // 	FString ShaderHashes = ShaderHashesToString(Shaders);

        // 	UE_LOG(LogVulkanRHI, Error, TEXT("Failed to create graphics pipeline.\nShaders in pipeline: %s"), *ShaderHashes);
        return false;
    }

    // double EndTime = FPlatformTime::Seconds();
    // double Delta = EndTime - BeginTime;
    // if (Delta > HitchTime)
    // {
    // 	UE_LOG(LogVulkanRHI, Verbose, TEXT("Hitchy gfx pipeline key CS (%.3f ms)"), (float)(Delta * 1000.0));
    // }

    // INC_DWORD_STAT(STAT_VulkanNumPSOs);
    return true;
}

VkResult PipelineStateCacheManager::CreateVKPipeline(VulkanGraphicsPipelineState *PSO, VulkanShader *Shaders[ShaderStage::NumStages], const VkGraphicsPipelineCreateInfo &PipelineInfo, bool bIsPrecompileJob)
{
    VkPipeline *Pipeline = &PSO->VulkanPipeline;
    VkResult Result = vkCreateGraphicsPipelines(device->GetInstanceHandle(), VK_NULL_HANDLE,
                                                1, &PipelineInfo, VULKAN_CPU_ALLOCATOR, Pipeline);
    return Result;
}

// 2197
VulkanComputePipeline *PipelineStateCacheManager::CreateComputePipelineState(ComputeShader *ComputeShaderRHI)
{
    VulkanComputeShader *shader = static_cast<VulkanComputeShader *>(ComputeShaderRHI);
    return device->GetPipelineStateCache()->GetOrCreateComputePipeline(shader);
}

ComputePipelineState *RHI::CreateComputePipelineState(ComputeShader *shader)
{
    return device->PipelineStateCache->CreateComputePipelineState(shader);
}

VulkanComputePipeline *PipelineStateCacheManager::GetOrCreateComputePipeline(VulkanComputeShader *ComputeShader)
{
    check(ComputeShader);
    const uint64 Key = ComputeShader->GetShaderKey();
    {
        /* FRWScopeLock ScopeLock(ComputePipelineLock, SLT_ReadOnly); */
        auto it = ComputePipelineEntries.find(Key);
        VulkanComputePipeline **ComputePipelinePtr = it == ComputePipelineEntries.end() ? nullptr : &it->second;
        if (ComputePipelinePtr)
        {
            return *ComputePipelinePtr;
        }
    }

    VulkanComputePipeline *ComputePipeline = CreateComputePipelineFromShader(ComputeShader);

    {
        /* FRWScopeLock ScopeLock(ComputePipelineLock, SLT_Write); */
        if (0 == ComputePipelineEntries.count(Key))
        {
            ComputePipelineEntries[Key] = ComputePipeline;
        }
    }

    return ComputePipeline;
}

VulkanComputePipeline *PipelineStateCacheManager::CreateComputePipelineFromShader(VulkanComputeShader *Shader)
{
    VulkanComputePipeline *Pipeline = new VulkanComputePipeline(device);

    Pipeline->computeShader = Shader;
    Pipeline->computeShader->AddRef();

    DescriptorSetsLayoutInfo DescriptorSetLayoutInfo;
    const ShaderHeader &CSHeader = Shader->GetCodeHeader();
    UniformBufferGatherInfo UBGatherInfo;
    DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_COMPUTE_BIT, ShaderStage::Compute, CSHeader, UBGatherInfo);
    DescriptorSetLayoutInfo.FinalizeBindings<true>(*device, UBGatherInfo, std::vector<SamplerState *>());
    VulkanPipelineLayout *Layout = FindOrAddLayout(DescriptorSetLayoutInfo, false);
    VulkanComputeLayout *ComputeLayout = (VulkanComputeLayout *)Layout;
    if (!ComputeLayout->ComputePipelineDescriptorInfo.IsInitialized())
    {
        ComputeLayout->ComputePipelineDescriptorInfo.Initialize(Layout->GetDescriptorSetsLayout().RemappingInfo);
    }

    std::shared_ptr<ShaderModule> shaderModule = Shader->GetOrCreateHandle(Layout, Layout->GetDescriptorSetLayoutHash());

    VkComputePipelineCreateInfo PipelineInfo;
    ZeroVulkanStruct(PipelineInfo, VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
    PipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    PipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    PipelineInfo.stage.module = shaderModule->GetVkShaderModule();
    // main_00000000_00000000
    char EntryPoint[24];
    Shader->GetEntryPoint(EntryPoint, 24);
    PipelineInfo.stage.pName = EntryPoint;
    PipelineInfo.layout = ComputeLayout->GetPipelineLayout();

    if (device->SupportsBindless())
    {
        PipelineInfo.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    }

    VkPipelineShaderStageRequiredSubgroupSizeCreateInfo RequiredSubgroupSizeCreateInfo;
    if ((CSHeader.WaveSize > 0) && device->GetOptionalExtensions().HasEXTSubgroupSizeControl)
    {
        check(0);
    }

    VkResult Result;
    {
        /*         FScopedPipelineCache PipelineCacheShared = GlobalPSOCache.Get(EPipelineCacheAccess::Shared); */
        Result = vkCreateComputePipelines(device->GetInstanceHandle(), VK_NULL_HANDLE,
                                          1, &PipelineInfo, VULKAN_CPU_ALLOCATOR, &Pipeline->Pipeline);
    }

    if (Result != VK_SUCCESS)
    {
        check(0);
        /* std::string ComputeHash = Shader->GetHash().ToString();
        UE_LOG(LogVulkanRHI, Error, TEXT("Failed to create compute pipeline.\nShaders in pipeline: CS: %s"), *ComputeHash);
        Pipeline->SetValid(false); */
    }

    Pipeline->Layout = ComputeLayout;

    return Pipeline;
}

void PipelineStateCacheManager::NotifyDeletedComputePipeline(VulkanComputePipeline *Pipeline)
{
    if (Pipeline->computeShader)
    {
        const uint64 Key = Pipeline->computeShader->GetShaderKey();
        /* FRWScopeLock ScopeLock(ComputePipelineLock, SLT_Write);  */
        ComputePipelineEntries.erase(Key);
    }
}

// 2487
bool PipelineStateCacheManager::LRUEvictImmediately()
{
    return bEvictImmediately && CVarEnableLRU != 0;
}

void PipelineStateCacheManager::LRUTrim(uint32 nSpaceNeeded)
{
    if (!bUseLRU)
    {
        return;
    }
    check(0);
}

void PipelineStateCacheManager::LRUAdd(VulkanGraphicsPipelineState *PSO)
{
    if (!bUseLRU)
    {
        return;
    }
    check(0);
}

// 2546
void PipelineStateCacheManager::LRUTouch(VulkanGraphicsPipelineState *PSO)
{
    if (!bUseLRU)
    {
        return;
    }
    printf("ERROR: Don't support LRU %s %d\n", __FILE__, __LINE__);
}

void VulkanGraphicsPipelineState::GetOrCreateShaderModules(std::shared_ptr<ShaderModule> (&ShaderModulesOUT)[ShaderStage::NumStages], VulkanShader *const *Shaders)
{
    for (int32 Index = 0; Index < ShaderStage::NumStages; ++Index)
    {
        (!ShaderModulesOUT[Index]->IsValid());
        VulkanShader *Shader = Shaders[Index];
        if (Shader)
        {
            ShaderModulesOUT[Index] = Shader->GetOrCreateHandle(Desc, Layout, Layout->GetDescriptorSetLayoutHash());
        }
    }
}

// 2602
void VulkanGraphicsPipelineState::DeleteVkPipeline(bool bImmediate)
{
    if (VulkanPipeline != VK_NULL_HANDLE)
    {
        if (bImmediate)
        {
            vkDestroyPipeline(device->GetInstanceHandle(), VulkanPipeline, VULKAN_CPU_ALLOCATOR);
        }
        else
        {
            device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::DeferredDeletionQueue2::EType::Pipeline, VulkanPipeline);
        }
        VulkanPipeline = VK_NULL_HANDLE;
    }

    device->PipelineStateCache->LRUCheckNotInside(this);
}

void PipelineStateCacheManager::LRUCheckNotInside(VulkanGraphicsPipelineState *PSO)
{
    printf("Have not implement PipelineStateCacheManager::LRUCheckNotInside %s %d\n", __FILE__, __LINE__);
}
