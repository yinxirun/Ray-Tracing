#include "pipeline.h"
#include "rhi.h"
#include "device.h"
#include "context.h"

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

VulkanGraphicsPipelineState::VulkanGraphicsPipelineState(Device *device, const GraphicsPipelineStateInitializer &PSOInitializer_,
                                                         GfxPipelineDesc &Desc, VulkanPSOKey *VulkanKey)
    // : bIsRegistered(false), PrimitiveType(PSOInitializer_.PrimitiveType),
    : VulkanPipeline(0), device(device), Desc(Desc) //, VulkanKey(VulkanKey->CopyDeep())
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
    // #if !UE_BUILD_SHIPPING
    //     SGraphicsRHICount--;
    // #endif
    //     DEC_DWORD_STAT(STAT_VulkanNumGraphicsPSOs);

    //     for (int ShaderStageIndex = 0; ShaderStageIndex < ShaderStage::NumStages; ShaderStageIndex++)
    //     {
    //         if (VulkanShaders[ShaderStageIndex] != nullptr)
    //         {
    //             VulkanShaders[ShaderStageIndex]->Release();
    //         }
    //     }

    //     device->PipelineStateCache->NotifyDeletedGraphicsPSO(this);
}

// 2042
std::shared_ptr<GraphicsPipelineState> PipelineStateCacheManager::CreateGraphicsPipelineState(const GraphicsPipelineStateInitializer &Initializer)
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
    // FVulkanDescriptorSetsLayoutInfo DescriptorSetLayoutInfo;
    {
        // 	CreateGfxEntry(Initializer, DescriptorSetLayoutInfo, &Desc);
        // 	Key = Desc.CreateKey2();
    }

    VulkanGraphicsPipelineState *NewPSO = 0;
    {
        // 	{
        // 		VulkanGraphicsPipelineState** PSO = GraphicsPSOLockedMap.Find(Key);
        // 		if(PSO)
        // 		{
        // 			check(*PSO);
        // 			if(!bIsPrecache)
        // 			{
        // 				LRUTouch(*PSO);
        // 			}
        // 			return *PSO;
        // 		}
        // 	}
    }

    // {
    // 	// Workers can be creating PSOs while FRHIResource::FlushPendingDeletes is running on the RHI thread
    // 	// so let it get enqueued for a delete with Release() instead.  Only used for failed or duplicate PSOs...
    // 	auto DeleteNewPSO = [](FVulkanRHIGraphicsPipelineState* PSOPtr)
    // 	{
    // 		PSOPtr->AddRef();
    // 		const uint32 RefCount = PSOPtr->Release();
    // 		check(RefCount == 0);
    // 	};

    // 	SCOPE_CYCLE_COUNTER(STAT_VulkanPSOCreationTime);
    NewPSO = new VulkanGraphicsPipelineState(device, Initializer, Desc, &Key);
    {

        // 		FVulkanLayout* Layout = FindOrAddLayout(DescriptorSetLayoutInfo, true);
        // 		FVulkanGfxLayout* GfxLayout = (FVulkanGfxLayout*)Layout;
        // 		check(GfxLayout->GfxPipelineDescriptorInfo.IsInitialized());
        // 		NewPSO->Layout = GfxLayout;
        // 		NewPSO->bHasInputAttachments = GfxLayout->GetDescriptorSetsLayout().HasInputAttachments();
    }
    NewPSO->RenderPass = device->GetImmediateContext().PrepareRenderPassForPSOCreation(Initializer);
    // 	{
    // 		const FBoundShaderStateInput& BSI = Initializer.BoundShaderState;
    // 		for (int32 StageIdx = 0; StageIdx < ShaderStage::NumStages; ++StageIdx)
    // 		{
    // 			NewPSO->ShaderKeys[StageIdx] = GetShaderKeyForGfxStage(BSI, (ShaderStage::EStage)StageIdx);
    // 		}

    // 		check(BSI.VertexShaderRHI);
    // 		FVulkanVertexShader* VS = ResourceCast(BSI.VertexShaderRHI);
    // 		const FVulkanShaderHeader& VSHeader = VS->GetCodeHeader();
    // 		NewPSO->VertexInputState.Generate(ResourceCast(Initializer.BoundShaderState.VertexDeclarationRHI), VSHeader.InOutMask);

    // 		if((!bIsPrecache || !LRUEvictImmediately())
    // #if !UE_BUILD_SHIPPING
    // 			&& 0 == CVarPipelineDebugForceEvictImmediately.GetValueOnAnyThread()
    // #endif
    // 			)
    // 		{

    // Create the pipeline
    /*double BeginTime = PlatformTime::Seconds();*/
    // 			FVulkanShader* VulkanShaders[ShaderStage::NumStages];
    // 			GetVulkanShaders(Initializer.BoundShaderState, VulkanShaders);

    // 			for (int32 StageIdx = 0; StageIdx < ShaderStage::NumStages; ++StageIdx)
    // 			{
    // 				uint64 key = GetShaderKeyForGfxStage(BSI, (ShaderStage::EStage)StageIdx);
    // 				check(key == NewPSO->ShaderKeys[StageIdx]);
    // 			}

    // 			QUICK_SCOPE_CYCLE_COUNTER(STAT_Vulkan_RHICreateGraphicsPipelineState_CREATE_PART0);

    // 			if(!CreateGfxPipelineFromEntry(NewPSO, VulkanShaders, bIsPrecache))
    // 			{
    // 				DeleteNewPSO(NewPSO);
    // 				return nullptr;
    // 			}
    // 			// Recover if we failed to create the pipeline.
    // 			double EndTime = FPlatformTime::Seconds();
    // 			double Delta = EndTime - BeginTime;
    // 			if (Delta > HitchTime)
    // 			{
    // 				UE_LOG(LogVulkanRHI, Verbose, TEXT("Hitchy gfx pipeline (%.3f ms)"), (float)(Delta * 1000.0));
    // 			}
    // 		}
    // 		FScopeLock Lock(&GraphicsPSOLockedCS);
    // 		FVulkanRHIGraphicsPipelineState** MapPSO = GraphicsPSOLockedMap.Find(Key);
    // 		if(MapPSO)//another thread could end up creating it.
    // 		{
    // 			DeleteNewPSO(NewPSO);
    // 			NewPSO = *MapPSO;
    // 		}
    // 		else
    // 		{
    // 			GraphicsPSOLockedMap.Add(MoveTemp(Key), NewPSO);
    // 			if (bUseLRU && NewPSO->VulkanPipeline != VK_NULL_HANDLE)
    // 			{
    // 				QUICK_SCOPE_CYCLE_COUNTER(STAT_Vulkan_RHICreateGraphicsPipelineState_LRU_PSOLock);
    // 				// we add only created pipelines to the LRU
    // 				FScopeLock LockRU(&LRUCS);
    // 				NewPSO->bIsRegistered = true;
    // 				LRUTrim(NewPSO->PipelineCacheSize);
    // 				LRUAdd(NewPSO);
    // 			}
    // 			else
    // 			{
    // 				NewPSO->bIsRegistered = true;
    // 			}
    // 		}
    // 	}
    // }
    return std::shared_ptr<VulkanGraphicsPipelineState>(NewPSO);
}