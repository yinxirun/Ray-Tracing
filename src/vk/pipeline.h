#pragma once

#include "core/containers/enum_as_byte.h"
#include "resources.h"
#include "private.h"
#include "descriptor_sets.h"
#include "util.h"
#include <memory>

#define VULKAN_USE_SHADERKEYS 1

class VulkanGraphicsPipelineState;
class VulkanGfxLayout;
class Device;
class DescriptorSetsLayoutInfo;

class VulkanPSOKey : public TDataKey<VulkanPSOKey, true>
{
};
namespace std
{
    template <>
    struct hash<VulkanPSOKey>
    {
        size_t operator()(const VulkanPSOKey &v) const
        {
            return v.GetHash();
        }
    };
}

template <typename TVulkanShader>
static inline uint64 GetShaderKey(GraphicsShader *ShaderType)
{
    TVulkanShader *vulkanShader = static_cast<TVulkanShader *>(ShaderType);
    VulkanShader *BaseVulkanShader = static_cast<VulkanShader *>(vulkanShader);
    return BaseVulkanShader ? BaseVulkanShader->GetShaderKey() : 0;
}

inline uint64 GetShaderKeyForGfxStage(const BoundShaderStateInput &BSI, ShaderStage::Stage Stage)
{
    switch (Stage)
    {
    case ShaderStage::Vertex:
        return GetShaderKey<VulkanVertexShader>(BSI.VertexShaderRHI);
    case ShaderStage::Pixel:
        return GetShaderKey<VulkanPixelShader>(BSI.PixelShaderRHI);
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
    case ShaderStage::Geometry:
        return GetShaderKey<FVulkanGeometryShader>(BSI.GetGeometryShader());
#endif
#if RHI_RAYTRACING
    case ShaderStage::RayGen:
    case ShaderStage::RayHitGroup:
    case ShaderStage::RayMiss:
    case ShaderStage::RayCallable:
        return 0; // VKRT todo
#endif
    default:
        check(0);
    }

    return 0;
}

// 111
struct DescriptorSetLayoutBinding
{
    uint32 Binding;
    uint8 DescriptorType;
    uint8 StageFlags;

    void ReadFrom(const VkDescriptorSetLayoutBinding &InState);
    void WriteInto(VkDescriptorSetLayoutBinding &OutState) const;

    bool operator==(const DescriptorSetLayoutBinding &In) const
    {
        return Binding == In.Binding &&
               DescriptorType == In.DescriptorType &&
               StageFlags == In.StageFlags;
    }
};

struct GfxPipelineDesc
{
    VulkanPSOKey CreateKey2() const;

    uint32 VertexInputKey = 0;
    uint16 RasterizationSamples;
    uint16 ControlPoints;
    uint32 Topology;
    struct BlendAttachment
    {
        bool bBlend;
        uint8 ColorBlendOp;
        uint8 SrcColorBlendFactor;
        uint8 DstColorBlendFactor;
        uint8 AlphaBlendOp;
        uint8 SrcAlphaBlendFactor;
        uint8 DstAlphaBlendFactor;
        uint8 ColorWriteMask;
        void ReadFrom(const VkPipelineColorBlendAttachmentState &InState);
        void WriteInto(VkPipelineColorBlendAttachmentState &OutState) const;
        bool operator==(const BlendAttachment &In) const
        {
            return bBlend == In.bBlend &&
                   ColorBlendOp == In.ColorBlendOp &&
                   SrcColorBlendFactor == In.SrcColorBlendFactor &&
                   DstColorBlendFactor == In.DstColorBlendFactor &&
                   AlphaBlendOp == In.AlphaBlendOp &&
                   SrcAlphaBlendFactor == In.SrcAlphaBlendFactor &&
                   DstAlphaBlendFactor == In.DstAlphaBlendFactor &&
                   ColorWriteMask == In.ColorWriteMask;
        }
    };
    std::vector<BlendAttachment> ColorAttachmentStates;

    std::vector<std::vector<DescriptorSetLayoutBinding>> DescriptorSetLayoutBindings;

    struct VertexBinding
    {
        uint32 Stride;
        uint16 Binding;
        uint16 InputRate;
        void ReadFrom(const VkVertexInputBindingDescription &InState);
        void WriteInto(VkVertexInputBindingDescription &OutState) const;
        bool operator==(const VertexBinding &In) const
        {
            return Stride == In.Stride &&
                   Binding == In.Binding &&
                   InputRate == In.InputRate;
        }
    };
    std::vector<VertexBinding> VertexBindings;

    struct VertexAttribute
    {
        uint32 Location;
        uint32 Binding;
        uint32 Format;
        uint32 Offset;
        void ReadFrom(const VkVertexInputAttributeDescription &InState);
        void WriteInto(VkVertexInputAttributeDescription &OutState) const;
        bool operator==(const VertexAttribute &In) const
        {
            return Location == In.Location &&
                   Binding == In.Binding &&
                   Format == In.Format &&
                   Offset == In.Offset;
        }
    };
    std::vector<VertexAttribute> VertexAttributes;

    struct Rasterizer
    {
        uint8 PolygonMode;
        uint8 CullMode;
        float DepthBiasSlopeScale;
        float DepthBiasConstantFactor;

        void ReadFrom(const VkPipelineRasterizationStateCreateInfo &InState);
        void WriteInto(VkPipelineRasterizationStateCreateInfo &OutState) const;

        bool operator==(const Rasterizer &In) const
        {
            return PolygonMode == In.PolygonMode &&
                   CullMode == In.CullMode &&
                   DepthBiasSlopeScale == In.DepthBiasSlopeScale &&
                   DepthBiasConstantFactor == In.DepthBiasConstantFactor;
        }
    };
    Rasterizer rasterizer;

    struct FDepthStencil
    {
        uint8 DepthCompareOp;
        bool bDepthTestEnable;
        bool bDepthWriteEnable;
        bool bStencilTestEnable;
        bool bDepthBoundsTestEnable;
        uint8 FrontFailOp;
        uint8 FrontPassOp;
        uint8 FrontDepthFailOp;
        uint8 FrontCompareOp;
        uint32 FrontCompareMask;
        uint32 FrontWriteMask;
        uint32 FrontReference;
        uint8 BackFailOp;
        uint8 BackPassOp;
        uint8 BackDepthFailOp;
        uint8 BackCompareOp;
        uint32 BackCompareMask;
        uint32 BackWriteMask;
        uint32 BackReference;

        void ReadFrom(const VkPipelineDepthStencilStateCreateInfo &InState);
        void WriteInto(VkPipelineDepthStencilStateCreateInfo &OutState) const;

        bool operator==(const FDepthStencil &In) const
        {
            return DepthCompareOp == In.DepthCompareOp &&
                   bDepthTestEnable == In.bDepthTestEnable &&
                   bDepthWriteEnable == In.bDepthWriteEnable &&
                   bDepthBoundsTestEnable == In.bDepthBoundsTestEnable &&
                   bStencilTestEnable == In.bStencilTestEnable &&
                   FrontFailOp == In.FrontFailOp &&
                   FrontPassOp == In.FrontPassOp &&
                   FrontDepthFailOp == In.FrontDepthFailOp &&
                   FrontCompareOp == In.FrontCompareOp &&
                   FrontCompareMask == In.FrontCompareMask &&
                   FrontWriteMask == In.FrontWriteMask &&
                   FrontReference == In.FrontReference &&
                   BackFailOp == In.BackFailOp &&
                   BackPassOp == In.BackPassOp &&
                   BackDepthFailOp == In.BackDepthFailOp &&
                   BackCompareOp == In.BackCompareOp &&
                   BackCompareMask == In.BackCompareMask &&
                   BackWriteMask == In.BackWriteMask &&
                   BackReference == In.BackReference;
        }
    };
    FDepthStencil DepthStencil;

#if VULKAN_USE_SHADERKEYS
    uint64 ShaderKeys[ShaderStage::NumStages];
    uint64 ShaderKeyShared;
#else
    FVulkanShaderHashes ShaderHashes;
#endif

    struct FRenderTargets
    {
        struct FAttachmentRef
        {
            uint32 Attachment;
            uint64 Layout;

            void ReadFrom(const VkAttachmentReference &InState);
            void WriteInto(VkAttachmentReference &OutState) const;
            bool operator==(const FAttachmentRef &In) const
            {
                return Attachment == In.Attachment && Layout == In.Layout;
            }
        };

        struct FStencilAttachmentRef
        {
            uint64 Layout;

            void ReadFrom(const VkAttachmentReferenceStencilLayout &InState);
            void WriteInto(VkAttachmentReferenceStencilLayout &OutState) const;
            bool operator==(const FStencilAttachmentRef &In) const
            {
                return Layout == In.Layout;
            }
        };

        std::vector<FAttachmentRef> ColorAttachments;
        std::vector<FAttachmentRef> ResolveAttachments;
        FAttachmentRef Depth;
        FStencilAttachmentRef Stencil;
        FAttachmentRef FragmentDensity;

        struct FAttachmentDesc
        {
            uint32 Format;
            uint8 Flags;
            uint8 Samples;
            uint8 LoadOp;
            uint8 StoreOp;
            uint8 StencilLoadOp;
            uint8 StencilStoreOp;
            uint64 InitialLayout;
            uint64 FinalLayout;

            bool operator==(const FAttachmentDesc &In) const
            {
                return Format == In.Format &&
                       Flags == In.Flags &&
                       Samples == In.Samples &&
                       LoadOp == In.LoadOp &&
                       StoreOp == In.StoreOp &&
                       StencilLoadOp == In.StencilLoadOp &&
                       StencilStoreOp == In.StencilStoreOp &&
                       InitialLayout == In.InitialLayout &&
                       FinalLayout == In.FinalLayout;
            }

            void ReadFrom(const VkAttachmentDescription &InState);
            void WriteInto(VkAttachmentDescription &OutState) const;
        };

        struct StencilAttachmentDesc
        {
            uint64 InitialLayout;
            uint64 FinalLayout;

            bool operator==(const StencilAttachmentDesc &In) const
            {
                return InitialLayout == In.InitialLayout &&
                       FinalLayout == In.FinalLayout;
            }

            void ReadFrom(const VkAttachmentDescriptionStencilLayout &InState);
            void WriteInto(VkAttachmentDescriptionStencilLayout &OutState) const;
        };

        std::vector<FAttachmentDesc> Descriptions;
        StencilAttachmentDesc StencilDescription;

        uint8 NumAttachments;
        uint8 NumColorAttachments;
        uint8 bHasDepthStencil;
        uint8 bHasResolveAttachments;
        uint8 bHasFragmentDensityAttachment;
        uint8 NumUsedClearValues;
        uint32 RenderPassCompatibleHash;
        IntVec3 Extent3D;

        void ReadFrom(const RenderTargetLayout &InState);
        void WriteInto(RenderTargetLayout &OutState) const;

        bool operator==(const FRenderTargets &In) const
        {
            return ColorAttachments == In.ColorAttachments &&
                   ResolveAttachments == In.ResolveAttachments &&
                   Depth == In.Depth &&
                   Stencil == In.Stencil &&
                   FragmentDensity == In.FragmentDensity &&
                   Descriptions == In.Descriptions &&
                   StencilDescription == In.StencilDescription &&
                   NumAttachments == In.NumAttachments &&
                   NumColorAttachments == In.NumColorAttachments &&
                   bHasDepthStencil == In.bHasDepthStencil &&
                   bHasResolveAttachments == In.bHasResolveAttachments &&
                   bHasFragmentDensityAttachment == In.bHasFragmentDensityAttachment &&
                   NumUsedClearValues == In.NumUsedClearValues &&
                   RenderPassCompatibleHash == In.RenderPassCompatibleHash &&
                   Extent3D == In.Extent3D;
        }
    };
    FRenderTargets RenderTargets;

    uint8 SubpassIndex;

    uint8 UseAlphaToCoverage;

    VRSShadingRate ShadingRate = VRSShadingRate::VRSSR_1x1;
    VRSRateCombiner Combiner = VRSRateCombiner::VRSRB_Passthrough;

    bool operator==(const GfxPipelineDesc &In) const
    {
        if (VertexInputKey != In.VertexInputKey)
            return false;
        if (RasterizationSamples != In.RasterizationSamples)
            return false;
        if (Topology != In.Topology)
            return false;
        if (ColorAttachmentStates != In.ColorAttachmentStates)
            return false;
        if (DescriptorSetLayoutBindings != In.DescriptorSetLayoutBindings)
            return false;
        if (!(rasterizer == In.rasterizer))
            return false;
        if (!(DepthStencil == In.DepthStencil))
            return false;
        if (!(SubpassIndex == In.SubpassIndex))
            return false;
        if (!(UseAlphaToCoverage == In.UseAlphaToCoverage))
            return false;

#if 0 == VULKAN_USE_SHADERKEYS
            		if (!(ShaderHashes == In.ShaderHashes))
            		{
            			return false;
            		}
#else
        if (0 != memcmp(ShaderKeys, In.ShaderKeys, sizeof(ShaderKeys)))
            return false;
#endif

        if (!(RenderTargets == In.RenderTargets))
            return false;
        if (VertexBindings != In.VertexBindings)
            return false;
        if (VertexAttributes != In.VertexAttributes)
            return false;
        if (ShadingRate != In.ShadingRate)
            return false;
        if (Combiner != In.Combiner)
            return false;

        return true;
    }
};

class VulkanComputePipeline;
class PipelineStateCacheManager
{
public:
    // Array of potential cache locations; first entries have highest priority. Only one cache file is loaded. If unsuccessful, tries next entry in the array.
    void InitAndLoad(const std::vector<std::string> &CacheFilenames);
    // 	void Save(const FString& CacheFilename);

    PipelineStateCacheManager(Device *InParent);

    ~PipelineStateCacheManager();

    // 	void RebuildCache();

    VulkanComputePipeline *GetOrCreateComputePipeline(VulkanComputeShader *ComputeShader);
    void NotifyDeletedComputePipeline(VulkanComputePipeline *Pipeline);

private:
    // 	class FPipelineCache;

    // 	/** Delegate handlers to track the ShaderPipelineCache precompile. */
    // 	void OnShaderPipelineCacheOpened(FString const& Name, EShaderPlatform Platform, uint32 Count, const FGuid& VersionGuid, FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext);
    // 	void OnShaderPipelineCachePrecompilationComplete(uint32 Count, double Seconds, const FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext);

    void CreateGfxEntry(const GraphicsPipelineStateInitializer &PSOInitializer, DescriptorSetsLayoutInfo &DescriptorSetLayoutInfo, GfxPipelineDesc *Desc);
    // 	bool Load(const TArray<FString>& CacheFilenames, FPipelineCache& Cache);
    // 	void SavePSOCache(const FString& CacheFilename, FPipelineCache& Cache);
    void DestroyCache();

    GraphicsPipelineState *CreateGraphicsPipelineState(const GraphicsPipelineStateInitializer &Initializer);
    VulkanComputePipeline *CreateComputePipelineState(ComputeShader *ComputeShaderRHI);
    void NotifyDeletedGraphicsPSO(GraphicsPipelineState *PSO);
    bool CreateGfxPipelineFromEntry(VulkanGraphicsPipelineState *PSO, VulkanShader *Shaders[ShaderStage::NumStages], bool bPrecompile);

    VkResult CreateVKPipeline(VulkanGraphicsPipelineState *PSO, VulkanShader *Shaders[ShaderStage::NumStages], const VkGraphicsPipelineCreateInfo &PipelineInfo, bool bIsPrecompileJob);

    // 	static FString ShaderHashesToString(FVulkanShader* Shaders[ShaderStage::NumStages]);

    VulkanPipelineLayout *FindOrAddLayout(const DescriptorSetsLayoutInfo &DescriptorSetLayoutInfo, bool bGfxLayout);
    VulkanComputePipeline *CreateComputePipelineFromShader(VulkanComputeShader *Shader);

    /** LRU Related functions */
    // 	void TickLRU();
    bool LRUEvictImmediately();
    void LRUTrim(uint32 nSpaceNeeded);
    void LRUAdd(VulkanGraphicsPipelineState *PSO);
    void LRUTouch(VulkanGraphicsPipelineState *PSO);
    // 	bool LRUEvictOne(bool bOnlyOld = false);
    // 	void LRURemoveAll();
    // 	void LRUDump();
    // 	void LRUDebugEvictAll(); //evict all that are safe to evict without stalling..
    // 	void LRURemove(FVulkanRHIGraphicsPipelineState* PSO);
    void LRUCheckNotInside(VulkanGraphicsPipelineState *PSO);

    Device *device;
    bool bEvictImmediately;
    // 	FString CompiledPSOCacheTopFolderPath;
    // 	FString CompiledPSOCacheFolderName;
    // 	FDelegateHandle OnShaderPipelineCacheOpenedDelegate;
    // 	FDelegateHandle OnShaderPipelineCachePrecompilationCompleteDelegate;

    // 	FRWLock ComputePipelineLock;
    std::unordered_map<uint64, VulkanComputePipeline *> ComputePipelineEntries;

    // template <typename TType>
    // class FScopedRWAccessor
    // {
    //     bool bWriteAccess;
    //     TType &ProtectedObj;
    //     FRWLock &RWLock;

    // public:
    //     FScopedRWAccessor(bool bWriteAccessIn, TType &ProtectedObjIn, FRWLock &RWLockIn) : bWriteAccess(bWriteAccessIn), ProtectedObj(ProtectedObjIn), RWLock(RWLockIn) { bWriteAccess ? RWLock.WriteLock() : RWLock.ReadLock(); }
    //     ~FScopedRWAccessor() { bWriteAccess ? RWLock.WriteUnlock() : RWLock.ReadUnlock(); }
    //     TType &Get() { return ProtectedObj; }
    // };

    // using FScopedPipelineCache = FScopedRWAccessor<VkPipelineCache>;

    // enum class EPipelineCacheAccess : uint8
    // {
    //     Shared,   // 'read' access, or for use when the API does its own synchronization.
    //     Exclusive // 'write' access, excludes all other usage for the duration.
    // };
    // class FPipelineCache
    // {
    //     VkPipelineCache PipelineCache = VK_NULL_HANDLE;
    //     FRWLock PipelineCacheLock;

    // public:
    //     FScopedPipelineCache Get(EPipelineCacheAccess PipelineAccessType) { return FScopedPipelineCache(PipelineAccessType == EPipelineCacheAccess::Exclusive, PipelineCache, PipelineCacheLock); }
    // };
    // 	FPipelineCache GlobalPSOCache;		// contains all PSO caches opened during the program run as well as PSO objects created on the fly

    // 	FPipelineCache CurrentPrecompilingPSOCache;
    // if true, we will link to the PSOFC, loading later, when we have that guid and only if the guid matches, saving only if there is no match, and only saving after the PSOFC is done.
    bool bPrecompilingCacheLoadedFromFile;
    // 	FGuid CurrentPrecompilingPSOCacheGuid;

    // 	TSet<FGuid> CompiledPSOCaches;

    // 	FCriticalSection LayoutMapCS;
    std::unordered_map<DescriptorSetsLayoutInfo, VulkanPipelineLayout *> LayoutMap;
    DescriptorSetLayoutMap DSetLayoutMap;

    // 	FCriticalSection GraphicsPSOLockedCS;
    std::unordered_map<VulkanPSOKey, VulkanGraphicsPipelineState *> GraphicsPSOLockedMap;

    // 	FCriticalSection LRUCS;
    // 	FVulkanRHIGraphicsPipelineStateLRU LRU;
    uint32 LRUUsedPipelineSize = 0;
    uint32 LRUUsedPipelineCount = 0;
    uint32 LRUUsedPipelineMax = 0;
    // 	TMap<uint64, FVulkanPipelineSize> LRU2SizeList;	// key: Shader hash (FShaderHash), value: pipeline size
    bool bUseLRU = false;
    friend class RHI;
    friend class CommandListContext;
    friend class VulkanGraphicsPipelineState;

    // 	struct FVulkanLRUCacheFile
    // 	{
    // 		enum
    // 		{
    // 			LRU_CACHE_VERSION = 2,
    // 		};
    // 		struct FFileHeader
    // 		{
    // 			int32 Version = -1;
    // 			int32 SizeOfPipelineSizes = -1;
    // 		} Header;

    // 		TArray<FVulkanPipelineSize> PipelineSizes;

    // 		void Save(FArchive& Ar);
    // 		bool Load(FArchive& Ar);
    // 	};
};

// Common pipeline class
class VulkanPipeline
{
public:
    VulkanPipeline(Device *InDevice);
    virtual ~VulkanPipeline();

    inline const VulkanPipelineLayout &GetLayout() const { return *Layout; }

protected:
    Device *device;
    VkPipeline Pipeline;
    VulkanPipelineLayout *Layout; /*owned by FVulkanPipelineStateCacheManager, do not delete yourself !*/

    friend class PipelineStateCacheManager;
    friend class VulkanGraphicsPipelineState;
    friend class VulkanComputePipelineDescriptorState;
};

class VulkanComputePipeline : public ComputePipelineState, public VulkanPipeline
{
public:
    VulkanComputePipeline(Device *InDevice);
    virtual ~VulkanComputePipeline();

    inline const ShaderHeader &GetShaderCodeHeader() const { return computeShader->GetCodeHeader(); }

    inline const VulkanComputeShader *GetShader() const { return computeShader; }

    inline void Bind(VkCommandBuffer CmdBuffer)
    {
        vkCmdBindPipeline(CmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);
    }

    inline const VulkanComputeLayout &GetComputeLayout() const { return *(VulkanComputeLayout *)Layout; }

protected:
    VulkanComputeShader *computeShader;
    friend class PipelineStateCacheManager;
};

class VulkanGraphicsPipelineState : public GraphicsPipelineState
{
public:
    VulkanGraphicsPipelineState(Device *Device, const GraphicsPipelineStateInitializer &PSOInitializer,
                                GfxPipelineDesc &Desc, VulkanPSOKey *Key);
    ~VulkanGraphicsPipelineState();

    inline const VulkanVertexInputStateInfo &GetVertexInputState() const { return VertexInputState; }

    inline const VulkanPipelineLayout &GetLayout() const { return *Layout; }

    inline const VulkanGfxLayout &GetGfxLayout() const { return *(VulkanGfxLayout *)&GetLayout(); }

    inline void Bind(VkCommandBuffer CmdBuffer)
    {
        vkCmdBindPipeline(CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanPipeline);
    }

    inline const uint64 GetShaderKey(ShaderFrequency Frequency) const
    {
        ShaderStage::Stage Stage = ShaderStage::GetStageForFrequency(Frequency);
        return ShaderKeys[Stage];
    }

    inline const VulkanShader *GetShader(ShaderFrequency Frequency) const
    {
        ShaderStage::Stage Stage = ShaderStage::GetStageForFrequency(Frequency);
        return VulkanShaders[Stage];
    }

    inline VkPipeline GetVulkanPipeline() const { return VulkanPipeline; }

    void DeleteVkPipeline(bool bImmediate);
    void GetOrCreateShaderModules(std::shared_ptr<ShaderModule> (&ShaderModulesOUT)[ShaderStage::NumStages], VulkanShader *const *Shaders);
    // VulkanShader::SpirvCode GetPatchedSpirvCode(VulkanShader *Shader);
    // void PurgeShaderModules(VulkanShader *const *Shaders);

    bool bHasInputAttachments = false;
    bool bIsRegistered;

    uint64 ShaderKeys[ShaderStage::NumStages];
    TEnumAsByte<PrimitiveType> PrimitiveType;

    VkPipeline VulkanPipeline;
    VulkanVertexInputStateInfo VertexInputState;
    VulkanPipelineLayout *Layout;
    Device *device;
    GfxPipelineDesc Desc;
    VulkanShader *VulkanShaders[ShaderStage::NumStages];
    const RenderPass *RenderPass;

    // FVulkanRHIGraphicsPipelineStateLRUNode *LRUNode = nullptr;
    uint32 LRUFrame = UINT32_MAX;
    uint32 PipelineCacheSize = UINT32_MAX;
    uint64 PrecacheKey; // hash of elements relevant to the PSO cache
    VulkanPSOKey VulkanKey;

#if VULKAN_PSO_CACHE_DEBUG
    FPixelShaderRHIRef PixelShaderRHI;
    FVertexShaderRHIRef VertexShaderRHI;
    FVertexDeclarationRHIRef VertexDeclarationRHI;

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    FGeometryShaderRHIRef GeometryShaderRHI;
#endif

    FGraphicsPipelineStateInitializer PSOInitializer;
#endif
};
