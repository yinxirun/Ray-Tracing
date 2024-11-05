#include "viewport.h"
#include "rhi.h"
#include <vector>
#include <memory>
#include "vulkan_memory.h"
#include "device.h"
#include "swap_chain.h"
#include "command_buffer.h"
#include "platform.h"
#include "barrier.h"
#include "context.h"
#include "resources.h"
#include "../definitions.h"
#include "private.h"
#include "queue.h"
#include "util.h"
#include "gpu/RHI/RHICommandList.h"
#include "gpu/RHI/RHIDefinitions.h"

BackBuffer::BackBuffer(Device &device, Viewport *InViewport, PixelFormat Format, uint32_t SizeX, uint32_t SizeY, ETextureCreateFlags UEFlags)
    : VulkanTexture(device, TextureCreateDesc::Create2D("FVulkanBackBuffer", SizeX, SizeY, Format).SetFlags(UEFlags).DetermineInititialState(), VK_NULL_HANDLE, false),
      viewport(InViewport)
{
}

BackBuffer::~BackBuffer()
{
    check(IsImageOwner() == false);
    // Clear ImageOwnerType so ~FVulkanTexture2D() doesn't try to re-destroy it
    ImageOwnerType = EImageOwnerType::None;
    ReleaseAcquiredImage();
}

void BackBuffer::OnGetBackBufferImage(RHICommandListImmediate &RHICmdList)
{
    check(viewport);
    if (GVulkanDelayAcquireImage == EDelayAcquireImageType::None)
    {
        CommandListContext &Context = (CommandListContext &)RHICmdList.GetContext();
        AcquireBackBufferImage(Context);
    }
}

void BackBuffer::ReleaseViewport()
{
    viewport = nullptr;
    ReleaseAcquiredImage();
}

void BackBuffer::ReleaseAcquiredImage()
{
    if (DefaultView)
    {
        // Do not invalidate view here, just remove a reference to it
        DefaultView = nullptr;
        PartialView = nullptr;
    }

    Image = VK_NULL_HANDLE;
}

void BackBuffer::AcquireBackBufferImage(CommandListContext &Context)
{
    check(viewport);

    if (Image == VK_NULL_HANDLE)
    {
        if (viewport->TryAcquireImageIndex())
        {
            int32 acquiredImageIndex = viewport->acquiredImageIndex;
            check(acquiredImageIndex >= 0 && acquiredImageIndex < viewport->textureViews.size());

            View &ImageView = *(viewport->textureViews[acquiredImageIndex]);

            Image = ImageView.GetTextureView().Image;
            DefaultView = &ImageView;
            PartialView = &ImageView;

            CommandBufferManager *CmdBufferManager = Context.GetCommandBufferManager();
            CmdBuffer *CmdBuffer = CmdBufferManager->GetActiveCmdBuffer();
            check(!CmdBuffer->IsInsideRenderPass());

            // right after acquiring image is in undefined state
            LayoutManager &LayoutMgr = CmdBuffer->GetLayoutManager();
            const ImageLayout CustomLayout(VK_IMAGE_LAYOUT_UNDEFINED, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
            LayoutMgr.SetFullLayout(Image, CustomLayout);

            // Wait for semaphore signal before writing to backbuffer image
            CmdBuffer->AddWaitSemaphore(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, viewport->acquiredSemaphore);
        }
        else
        {
            // fallback to a 'dummy' backbuffer
            check(viewport->renderingBackBuffer);
            View *DummyView = viewport->renderingBackBuffer->DefaultView;
            Image = DummyView->GetTextureView().Image;
            DefaultView = DummyView;
            PartialView = DummyView;
        }
    }
}

Viewport::Viewport(Device *d, void *InWindowHandle, uint32_t sizeX, uint32_t sizeY, PixelFormat InPreferredPixelFormat)
    : device(d), sizeX(sizeX), sizeY(sizeY), pixelFormat(InPreferredPixelFormat), acquiredImageIndex(-1),
      swapChain(nullptr), windowHandle(InWindowHandle), acquiredSemaphore(nullptr)
{
    RHI::Get().viewports.push_back(this);
    CreateSwapchain(nullptr);

    if (SupportsStandardSwapchain())
    {
        for (int32_t index = 0, numBuffers = renderingDoneSemaphores.size(); index < numBuffers; ++index)
        {
            renderingDoneSemaphores[index] = new VulkanRHI::Semaphore(*d);
            renderingDoneSemaphores[index]->AddRef();
        }
    }
}

Viewport::~Viewport()
{
    renderingBackBuffer = nullptr;

    if (RHIBackBuffer)
    {
        RHIBackBuffer->ReleaseViewport();
        RHIBackBuffer = nullptr;
    }

    if (SupportsStandardSwapchain())
    {
        for (auto view : textureViews)
            delete view;
        textureViews.clear();

        for (int32 Index = 0, NumBuffers = renderingDoneSemaphores.size(); Index < NumBuffers; ++Index)
        {
            renderingDoneSemaphores[Index]->Release();

            // FIXME: race condition on TransitionAndLayoutManager, could this be called from RT while RHIT is active?
            // device->NotifyDeletedImage(BackBufferImages[Index], true);
            backBufferImages[Index] = VK_NULL_HANDLE;
        }

        swapChain->Destroy(nullptr);
        delete swapChain;
        swapChain = nullptr;
    }

    std::vector<Viewport *> &viewports = RHI::Get().viewports;
    auto iter = std::find(viewports.begin(), viewports.end(), this);
    viewports.erase(iter);
}

std::shared_ptr<Texture> Viewport::GetBackBuffer(/*RHICommandListImmediate &RHICmdList*/)
{
    check(IsInRenderingThread());

    // make sure we aren't in the middle of swapchain recreation (which can happen on e.g. RHI thread)
    // FScopeLock LockSwapchain(&RecreatingSwapchain);

    if (SupportsStandardSwapchain() && GVulkanDelayAcquireImage != EDelayAcquireImageType::DelayAcquire)
    {
        printf("ERROR: Don't support EDelayAcquireImageType::DelayAcquire %s %d\n", __FILE__, __LINE__);
        exit(-1);
        // check(RHICmdList.IsImmediate());
        // check(RHIBackBuffer);

        // RHICmdList.EnqueueLambda([this](FRHICommandListImmediate &CmdList)
        //                          { this->RHIBackBuffer->OnGetBackBufferImage(CmdList); });

        // return RHIBackBuffer.GetReference();
    }

    return renderingBackBuffer;
}

inline static void CopyImageToBackBuffer(CommandListContext *context, CmdBuffer *cmdBuffer,
                                         VulkanTexture &srcSurface, VkImage dstSurface, int32 sizeX, int32 sizeY,
                                         int32 windowSizeX, int32 windowSizeY)
{
    LayoutManager &LayoutManager = cmdBuffer->GetLayoutManager();
    const VkImageLayout PreviousSrcLayout = LayoutManager::SetExpectedLayout(cmdBuffer, srcSurface, Access::CopySrc);

    {
        PipelineBarrier Barrier;
        Barrier.AddImageLayoutTransition(dstSurface,
                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         PipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1));
        Barrier.Execute(cmdBuffer);
    }

#ifdef PRINT_UNIMPLEMENT
    printf("Am i really need VulkanRHI::DebugHeavyWeightBarrier?? %s %d\n", __FILE__, __LINE__);
#endif
    // VulkanRHI::DebugHeavyWeightBarrier(CmdBuffer->GetHandle(), 32);

    if (sizeX != windowSizeX || sizeY != windowSizeY)
    {
        printf("Fuck %s %d\n", __FILE__, __LINE__);
        // VkImageBlit Region;
        // FMemory::Memzero(Region);
        // Region.srcOffsets[0].x = 0;
        // Region.srcOffsets[0].y = 0;
        // Region.srcOffsets[0].z = 0;
        // Region.srcOffsets[1].x = SizeX;
        // Region.srcOffsets[1].y = SizeY;
        // Region.srcOffsets[1].z = 1;
        // Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        // Region.srcSubresource.layerCount = 1;
        // Region.dstOffsets[0].x = 0;
        // Region.dstOffsets[0].y = 0;
        // Region.dstOffsets[0].z = 0;
        // Region.dstOffsets[1].x = WindowSizeX;
        // Region.dstOffsets[1].y = WindowSizeY;
        // Region.dstOffsets[1].z = 1;
        // Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        // Region.dstSubresource.baseArrayLayer = 0;
        // Region.dstSubresource.layerCount = 1;
        // VulkanRHI::vkCmdBlitImage(CmdBuffer->GetHandle(),
        //                           SrcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        //                           DstSurface, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        //                           1, &Region, VK_FILTER_LINEAR);
    }
    else
    {
        VkImageCopy Region{};
        Region.extent.width = sizeX;
        Region.extent.height = sizeY;
        Region.extent.depth = 1;
        Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Region.srcSubresource.layerCount = 1;
        Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Region.dstSubresource.layerCount = 1;
        vkCmdCopyImage(cmdBuffer->GetHandle(),
                       srcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       dstSurface, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &Region);
    }

    {
        PipelineBarrier Barrier;
        Barrier.AddImageLayoutTransition(srcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, PreviousSrcLayout, PipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1));
        Barrier.AddImageLayoutTransition(dstSurface, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, PipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1));
        Barrier.Execute(cmdBuffer);
    }
}

extern int32 GWaitForIdleOnSubmit;
void Viewport::WaitForFrameEventCompletion()
{
    if (Platform::RequiresWaitingForFrameCompletionEvent())
    {
        if (LastFrameCommandBuffer && LastFrameCommandBuffer->IsSubmitted())
        {
            // If last frame's fence hasn't been signaled already, wait for it here
            if (LastFrameFenceCounter == LastFrameCommandBuffer->GetFenceSignaledCounter())
            {
                if (!GWaitForIdleOnSubmit)
                {
                    // The wait has already happened if GWaitForIdleOnSubmit is set
                    LastFrameCommandBuffer->GetOwner()->GetMgr().WaitForCmdBuffer(LastFrameCommandBuffer);
                }
            }
        }
    }
}

void Viewport::IssueFrameEvent()
{
    if (Platform::RequiresWaitingForFrameCompletionEvent())
    {
        // The fence we need to wait on next frame is already there in the command buffer
        // that was just submitted in this frame's Present. Just grab that command buffer's
        // info to use next frame in WaitForFrameEventCompletion.
        Queue *queue = device->GetGraphicsQueue();
        queue->GetLastSubmittedInfo(LastFrameCommandBuffer, LastFrameFenceCounter);
    }
}

bool Viewport::Present(CommandListContext *context, CmdBuffer *cmdBuffer, Queue *queue, Queue *presentQueue, bool bLockToVsync)
{
    // FPlatformAtomics::AtomicStore(&LockToVsync, bLockToVsync ? 1 : 0);
    bool bFailedToDelayAcquireBackbuffer = false;

    // Transition back buffer to presentable and submit that command
    check(cmdBuffer->IsOutsideRenderPass());

    if (SupportsStandardSwapchain())
    {
        if (GVulkanDelayAcquireImage == EDelayAcquireImageType::DelayAcquire)
        {
            // swapchain can go out of date, do not crash at this point
            if (TryAcquireImageIndex())
            {
                uint32 WindowSizeX = std::min(sizeX, swapChain->internalWidth);
                uint32 WindowSizeY = std::min(sizeY, swapChain->internalHeight);

                context->RHIPushEvent("CopyImageToBackBuffer", 0);
                CopyImageToBackBuffer(context, cmdBuffer, *renderingBackBuffer.get(), backBufferImages[acquiredImageIndex],
                                      sizeX, sizeY, WindowSizeX, WindowSizeY);
                context->RHIPopEvent();
            }
            else
            {
                bFailedToDelayAcquireBackbuffer = true;
            }
        }
        else
        {
            if (acquiredImageIndex != -1)
            {
                check(RHIBackBuffer != nullptr && RHIBackBuffer->Image == backBufferImages[acquiredImageIndex]);

                LayoutManager &LayoutManager = cmdBuffer->GetLayoutManager();
                const ImageLayout *TrackedLayout = LayoutManager.GetFullLayout(backBufferImages[acquiredImageIndex]);

                // One of the rare cases we'll let parallel rendering check the tracking (legacy path already checked as a fallback)
                if (!TrackedLayout && device->SupportsParallelRendering())
                {
                    TrackedLayout = context->GetQueue()->GetLayoutManager().GetFullLayout(backBufferImages[acquiredImageIndex]);
                }

                VkImageLayout LastLayout = TrackedLayout ? TrackedLayout->MainLayout : VK_IMAGE_LAYOUT_UNDEFINED;
                VulkanSetImageLayout(cmdBuffer, backBufferImages[acquiredImageIndex], LastLayout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, PipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT));

                const ImageLayout UndefinedLayout(VK_IMAGE_LAYOUT_UNDEFINED, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
                LayoutManager.SetFullLayout(backBufferImages[acquiredImageIndex], UndefinedLayout);
            }
            else
            {
                // When we have failed to acquire backbuffer image we fallback to using 'dummy' backbuffer
                check(RHIBackBuffer != nullptr && RHIBackBuffer->Image == renderingBackBuffer->Image);
            }
        }
    }

    cmdBuffer->End();
    CommandBufferManager *ImmediateCmdBufMgr = device->GetImmediateContext().GetCommandBufferManager();
    ImmediateCmdBufMgr->FlushResetQueryPools();
    // checkf(ImmediateCmdBufMgr->GetActiveCmdBufferDirect() == CmdBuffer, TEXT("Present() is submitting something else than the active command buffer"));
    if (SupportsStandardSwapchain())
    {
        if (!bFailedToDelayAcquireBackbuffer)
        {
            if (GVulkanDelayAcquireImage == EDelayAcquireImageType::DelayAcquire)
            {
                cmdBuffer->AddWaitSemaphore(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, acquiredSemaphore);
            }

            VulkanRHI::Semaphore *signalSemaphore = (acquiredImageIndex >= 0 ? renderingDoneSemaphores[acquiredImageIndex] : nullptr);
            // submit through the CommandBufferManager as it will add the proper semaphore
            ImmediateCmdBufMgr->SubmitActiveCmdBufferFromPresent(signalSemaphore);
        }
        else
        {
            // failing to do the delayacquire can only happen if we were in this mode to begin with
            check(GVulkanDelayAcquireImage == EDelayAcquireImageType::DelayAcquire);

            printf("LOG: AcquireNextImage() failed due to the outdated swapchain, not even attempting to present.\n");

            // cannot just throw out this command buffer (needs to be submitted or other checks fail)
            queue->Submit(cmdBuffer);
            RecreateSwapchain(windowHandle);

            // Swapchain creation pushes some commands - flush the command buffers now to begin with a fresh state
            device->SubmitCommandsAndFlushGPU();
            device->WaitUntilIdle();

            // early exit
            return (int32)SwapChain::EStatus::Healthy;
        }
    }
    else
    {
        // Submit active command buffer if not supporting standard swapchain (e.g. XR devices).
        printf("Fuck!! %s %d\n", __FILE__, __LINE__);
        // ImmediateCmdBufMgr->SubmitActiveCmdBufferFromPresent(nullptr);
    }

    bool bNeedNativePresent = true;

    bool bResult = false;
    if (bNeedNativePresent && (!SupportsStandardSwapchain() || GVulkanDelayAcquireImage == EDelayAcquireImageType::DelayAcquire || RHIBackBuffer != nullptr))
    {
        // Present the back buffer to the viewport window.
        auto SwapChainJob = [queue, presentQueue](Viewport *Viewport) -> int32
        {
            // May happend if swapchain was recreated in DoCheckedSwapChainJob()
            if (Viewport->acquiredImageIndex == -1)
            {
                // Skip present silently if image has not been acquired
                return (int32)SwapChain::EStatus::Healthy;
            }
            return (int32)Viewport->swapChain->Present(queue, presentQueue, Viewport->renderingDoneSemaphores[Viewport->acquiredImageIndex]);
        };
        if (SupportsStandardSwapchain() && !DoCheckedSwapChainJob(SwapChainJob))
        {
            printf("Swapchain present failed! %s %d\n", __FILE__, __LINE__);
            bResult = false;
        }
        else
        {
            bResult = true;
        }
    }

    if (Platform::RequiresWaitingForFrameCompletionEvent())
    {
        // Wait for the GPU to finish rendering the previous frame before finishing this frame.
        WaitForFrameEventCompletion();
        IssueFrameEvent();
    }

    // PrepareForNewActiveCommandBuffer might be called by swapchain re-creation routine. Skip prepare if we already have an open active buffer.
    if (ImmediateCmdBufMgr->GetActiveCmdBuffer() && !ImmediateCmdBufMgr->GetActiveCmdBuffer()->HasBegun())
    {
        ImmediateCmdBufMgr->PrepareForNewActiveCommandBuffer();
    }

    acquiredImageIndex = -1;

    return bResult;
}

VkSurfaceTransformFlagBitsKHR Viewport::GetSwapchainQCOMRenderPassTransform() const
{
    return swapChain->QCOMRenderPassTransform;
}

void Viewport::CreateSwapchain(struct SwapChainRecreateInfo *recreateInfo)
{
    if (SupportsStandardSwapchain())
    {
        uint32_t desiredNumBackBuffers = NUM_BUFFERS;

        std::vector<VkImage> Images;

        if (pixelFormat == PixelFormat::PF_B8G8R8A8)
        {
            swapChain = new SwapChain(RHI::Get().instance, *device, windowHandle, VK_FORMAT_B8G8R8_UNORM, sizeX, sizeY, &desiredNumBackBuffers, Images, recreateInfo);
        }
        else
        {
            printf("%s: %s\n", __FILE__, __LINE__);
            exit(-1);
        }

        backBufferImages.resize(Images.size());
        renderingDoneSemaphores.resize(Images.size());

        CmdBuffer *cmdBuffer = device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();

        VkClearColorValue ClearColor;
        memset(&ClearColor, 0, sizeof(ClearColor));

        const VkImageSubresourceRange Range = PipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        const ImageLayout InitialLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
        for (int32_t Index = 0; Index < Images.size(); ++Index)
        {
            backBufferImages[Index] = Images[Index];
            const VkDescriptorType DescriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            textureViews.push_back((new View(*device, DescriptorType))->InitAsTextureView(Images[Index], VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, pixelFormat, UEToVkTextureFormat(pixelFormat, false), 0, 1, 0, 1, false));

            // Clear the swapchain to avoid a validation warning, and transition to PresentSrc

            SetImageLayout(cmdBuffer, Images[Index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Range);
            vkCmdClearColorImage(cmdBuffer->GetHandle(), Images[Index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &ClearColor, 1, &Range);
            SetImageLayout(cmdBuffer, Images[Index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, InitialLayout.MainLayout, Range);
            cmdBuffer->GetLayoutManager().SetFullLayout(Images[Index], InitialLayout);
        }

        device->GetImmediateContext().GetCommandBufferManager()->SubmitUploadCmdBuffer();

        RHIBackBuffer = std::make_shared<BackBuffer>(*device, this, pixelFormat, sizeX, sizeY, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_ResolveTargetable);
    }
    else
    {
        pixelFormat = GetPixelFormatForNonDefaultSwapchain();
        if (recreateInfo != nullptr)
        {
            if (recreateInfo->swapChain)
            {
                Platform::DestroySwapchainKHR(device->GetInstanceHandle(), recreateInfo->swapChain, nullptr);
                recreateInfo->swapChain = VK_NULL_HANDLE;
            }
            if (recreateInfo->surface)
            {
                vkDestroySurfaceKHR(RHI::Get().instance, recreateInfo->surface, nullptr);
                recreateInfo->surface = VK_NULL_HANDLE;
            }
        }
    }

    // We always create a 'dummy' backbuffer to gracefully handle SurfaceLost cases
    {
        uint32 BackBufferSizeX = RequiresRenderingBackBuffer() ? sizeX : 1;
        uint32 BackBufferSizeY = RequiresRenderingBackBuffer() ? sizeY : 1;

        ClearValueBinding clearValue;
        clearValue.Value.Color[0] = 1;
        clearValue.Value.Color[1] = 0;
        clearValue.Value.Color[2] = 0;
        clearValue.Value.Color[3] = 1;
        clearValue.ColorBinding = ClearBinding::ENoneBound;

        const TextureCreateDesc desc = TextureCreateDesc::Create2D("RenderingBackBuffer", BackBufferSizeX, BackBufferSizeY, pixelFormat)
                                           .SetClearValue(clearValue)
                                           .SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource | ETextureCreateFlags::ResolveTargetable)
                                           .SetInitialState(Access::Present);

        renderingBackBuffer = std::shared_ptr<VulkanTexture>(new VulkanTexture(*device, desc));
    }

    acquiredImageIndex = -1;
}

void Viewport::DestroySwapchain(struct SwapChainRecreateInfo *RecreateInfo)
{
    // Submit all command buffers here
    device->SubmitCommandsAndFlushGPU();
    device->WaitUntilIdle();

    if (RHIBackBuffer)
    {
        RHIBackBuffer->ReleaseAcquiredImage();
        // We release this RHIBackBuffer when we create a new swapchain
    }

    if (SupportsStandardSwapchain() && swapChain)
    {
        for (View *view : textureViews)
        {
            delete view;
        }
        textureViews.clear();

        for (int32 Index = 0, NumBuffers = backBufferImages.size(); Index < NumBuffers; ++Index)
        {
            device->NotifyDeletedImage(backBufferImages[Index], true);
            backBufferImages[Index] = VK_NULL_HANDLE;
        }

        device->GetDeferredDeletionQueue().ReleaseResources(true);

        swapChain->Destroy(RecreateInfo);
        delete swapChain;
        swapChain = nullptr;

        device->GetDeferredDeletionQueue().ReleaseResources(true);
    }

    acquiredImageIndex = -1;
}

bool Viewport::TryAcquireImageIndex()
{
    if (swapChain)
    {
        int32 result = swapChain->AcquireImageIndex(&acquiredSemaphore);
        if (result >= 0)
        {
            acquiredImageIndex = result;
            return true;
        }
    }
    return false;
}

Framebuffer::Framebuffer(Device &device, const SetRenderTargetsInfo &InRTInfo,
                         const RenderTargetLayout &RTLayout, const RenderPass &RenderPass)
    : framebuffer(VK_NULL_HANDLE), NumColorRenderTargets(InRTInfo.NumColorRenderTargets),
      NumColorAttachments(0), DepthStencilRenderTargetImage(VK_NULL_HANDLE), FragmentDensityImage(VK_NULL_HANDLE)
{
    memset(ColorRenderTargetImages, 0, sizeof(ColorRenderTargetImages));
    memset(ColorResolveTargetImages, 0, sizeof(ColorResolveTargetImages));

    AttachmentTextureViews.reserve(RTLayout.GetNumAttachmentDescriptions());

    auto CreateOwnedView = [&]()
    {
        const VkDescriptorType DescriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        View *view = new View(device, DescriptorType);
        AttachmentTextureViews.push_back(view);
        OwnedTextureViews.emplace_back(view);
        return view;
    };

    auto AddExternalView = [&](View const *View)
    {
        AttachmentTextureViews.push_back(View);
    };

    uint32 MipIndex = 0;

    const VkExtent3D &RTExtents = RTLayout.GetExtent3D();
    // Adreno does not like zero size RTs
    check(RTExtents.width != 0 && RTExtents.height != 0);
    uint32 NumLayers = RTExtents.depth;

    for (int32 Index = 0; Index < InRTInfo.NumColorRenderTargets; ++Index)
    {
        Texture *RHITexture = InRTInfo.ColorRenderTarget[Index].texture;
        if (!RHITexture)
        {
            continue;
        }

        VulkanTexture *texture = dynamic_cast<VulkanTexture *>(RHITexture);
        const TextureDesc &Desc = texture->GetDesc();

        // this could fire in case one of the textures is FVulkanBackBuffer and it has not acquired an image
        // with EDelayAcquireImageType::LazyAcquire acquire happens when texture transition to Writeable state
        // make sure you call TransitionResource(Writable, Tex) before using this texture as a render-target
        check(texture->Image != VK_NULL_HANDLE);

        ColorRenderTargetImages[Index] = texture->Image;
        MipIndex = InRTInfo.ColorRenderTarget[Index].MipIndex;

        if (texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D || texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
        {
            uint32 ArraySliceIndex, NumArraySlices;
            if (InRTInfo.ColorRenderTarget[Index].ArraySliceIndex == -1)
            {
                ArraySliceIndex = 0;
                NumArraySlices = texture->GetNumberOfArrayLevels();
            }
            else
            {
                ArraySliceIndex = InRTInfo.ColorRenderTarget[Index].ArraySliceIndex;
                NumArraySlices = 1;
                check(ArraySliceIndex < texture->GetNumberOfArrayLevels());
            }

            CreateOwnedView()->InitAsTextureView(
                texture->Image, texture->GetViewType(), texture->GetFullAspectMask(), Desc.Format,
                texture->ViewFormat, MipIndex, 1, ArraySliceIndex, NumArraySlices, true,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (texture->ImageUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT));
        }
        else if (texture->GetViewType() == VK_IMAGE_VIEW_TYPE_CUBE || texture->GetViewType() == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
        {
            CreateOwnedView()->InitAsTextureView(
                texture->Image, VK_IMAGE_VIEW_TYPE_2D, texture->GetFullAspectMask(), Desc.Format,
                texture->ViewFormat, MipIndex, 1, InRTInfo.ColorRenderTarget[Index].ArraySliceIndex, 1, true,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (texture->ImageUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT));
        }
        else if (texture->GetViewType() == VK_IMAGE_VIEW_TYPE_3D)
        {
            CreateOwnedView()->InitAsTextureView(
                texture->Image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, texture->GetFullAspectMask(), Desc.Format,
                texture->ViewFormat, MipIndex, 1, 0, Desc.Depth, true,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (texture->ImageUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT));
        }
        else
        {
            ensure(0);
        }

        ++NumColorAttachments;

        // Check the RTLayout as well to make sure the resolve attachment is needed (Vulkan and Feature level specific)
        // See: FVulkanRenderTargetLayout constructor with FRHIRenderPassInfo
        if (InRTInfo.bHasResolveAttachments &&
            RTLayout.GetHasResolveAttachments() &&
            RTLayout.GetResolveAttachmentReferences()[Index].layout != VK_IMAGE_LAYOUT_UNDEFINED)
        {
            auto *ResolveRHITexture = InRTInfo.ColorResolveRenderTarget[Index].texture;
            VulkanTexture *ResolveTexture = dynamic_cast<VulkanTexture *>(ResolveRHITexture);
            ColorResolveTargetImages[Index] = ResolveTexture->Image;

            // resolve attachments only supported for 2d/2d array textures
            if (ResolveTexture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D || ResolveTexture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
            {
                CreateOwnedView()->InitAsTextureView(
                    ResolveTexture->Image, ResolveTexture->GetViewType(),
                    ResolveTexture->GetFullAspectMask(),
                    ResolveTexture->GetDesc().Format,
                    ResolveTexture->ViewFormat,
                    MipIndex, 1,
                    std::max(0, (int32)InRTInfo.ColorRenderTarget[Index].ArraySliceIndex),
                    ResolveTexture->GetNumberOfArrayLevels(), true);
            }
        }
    }

    VkSurfaceTransformFlagBitsKHR QCOMRenderPassTransform = RTLayout.GetQCOMRenderPassTransform();

    if (RTLayout.GetHasDepthStencil())
    {
        VulkanTexture *texture = dynamic_cast<VulkanTexture *>(InRTInfo.DepthStencilRenderTarget.texture);
        const TextureDesc &Desc = texture->GetDesc();
        DepthStencilRenderTargetImage = texture->Image;
        bool bHasStencil = (texture->GetDesc().Format == PF_DepthStencil || texture->GetDesc().Format == PF_X24_G8);

        check(texture->PartialView);
        PartialDepthTextureView = texture->PartialView;

        ensure(texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D || texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY || texture->GetViewType() == VK_IMAGE_VIEW_TYPE_CUBE);
        if (NumColorAttachments == 0 && texture->GetViewType() == VK_IMAGE_VIEW_TYPE_CUBE)
        {
            CreateOwnedView()->InitAsTextureView(
                texture->Image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, texture->GetFullAspectMask(), texture->GetDesc().Format, texture->ViewFormat, MipIndex, 1, 0, 6, true);

            NumLayers = 6;
        }
        else if (texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D || texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
        {
            // depth attachments need a separate view to have no swizzle components, for validation correctness
            CreateOwnedView()->InitAsTextureView(
                texture->Image, texture->GetViewType(), texture->GetFullAspectMask(), texture->GetDesc().Format, texture->ViewFormat, MipIndex, 1, 0, texture->GetNumberOfArrayLevels(), true);
        }
        else if (QCOMRenderPassTransform != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR && Desc.Extent.x == RTExtents.width && Desc.Extent.y == RTExtents.height)
        {
            printf("Don't support Swap Chain Transform %s %d\n", __FILE__, __LINE__);
        }
        else
        {
            AddExternalView(texture->DefaultView);
        }
    }

    if (false /*GRHISupportsAttachmentVariableRateShading && GRHIVariableRateShadingEnabled && GRHIAttachmentVariableRateShadingEnabled && RTLayout.GetHasFragmentDensityAttachment()*/)
    {
        printf("Don't support VRS %s %d\n", __FILE__, __LINE__);
        //     FVulkanTexture *Texture = ResourceCast(InRTInfo.ShadingRateTexture);
        //     FragmentDensityImage = Texture->Image;

        //     ensure(Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D || Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY);

        //     CreateOwnedView()->InitAsTextureView(
        //         Texture->Image, Texture->GetViewType(), Texture->GetFullAspectMask(), Texture->GetDesc().Format, Texture->ViewFormat, MipIndex, 1, 0, Texture->GetNumberOfArrayLevels(), true);
    }

    std::vector<VkImageView> AttachmentViews;
    AttachmentViews.reserve(AttachmentTextureViews.size());
    for (View const *View : AttachmentTextureViews)
    {
        AttachmentViews.push_back(View->GetTextureView().View);
    }

    VkFramebufferCreateInfo CreateInfo;
    ZeroVulkanStruct(CreateInfo, VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
    CreateInfo.renderPass = RenderPass.GetHandle();
    CreateInfo.attachmentCount = AttachmentViews.size();
    CreateInfo.pAttachments = AttachmentViews.data();
    CreateInfo.width = RTExtents.width;
    CreateInfo.height = RTExtents.height;
    CreateInfo.layers = NumLayers;

    if (QCOMRenderPassTransform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR ||
        QCOMRenderPassTransform == VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
    {
        std::swap(CreateInfo.width, CreateInfo.height);
    }
    VERIFYVULKANRESULT_EXPANDED(vkCreateFramebuffer(device.GetInstanceHandle(), &CreateInfo, VULKAN_CPU_ALLOCATOR, &framebuffer));

    RenderArea.offset.x = 0;
    RenderArea.offset.y = 0;
    RenderArea.extent.width = RTExtents.width;
    RenderArea.extent.height = RTExtents.height;
}

Framebuffer::~Framebuffer() { ensure(framebuffer == VK_NULL_HANDLE); }

bool Framebuffer::Matches(const SetRenderTargetsInfo &InRTInfo) const
{
    if (NumColorRenderTargets != InRTInfo.NumColorRenderTargets)
        return false;

    {
        const RHIDepthRenderTargetView &B = InRTInfo.DepthStencilRenderTarget;
        if (B.texture)
        {
            VkImage AImage = DepthStencilRenderTargetImage;
            VkImage BImage = ResourceCast(B.texture)->Image;
            if (AImage != BImage)
            {
                return false;
            }
        }
    }

    {
        Texture *Texture = InRTInfo.ShadingRateTexture;
        if (Texture)
        {
            VkImage AImage = FragmentDensityImage;
            VkImage BImage = ResourceCast(Texture)->Image;
            if (AImage != BImage)
            {
                return false;
            }
        }
    }

    int32 AttachementIndex = 0;
    for (int32 Index = 0; Index < InRTInfo.NumColorRenderTargets; ++Index)
    {
        if (InRTInfo.bHasResolveAttachments)
        {
            const RHIRenderTargetView &R = InRTInfo.ColorResolveRenderTarget[Index];
            if (R.texture)
            {
                VkImage AImage = ColorResolveTargetImages[AttachementIndex];
                VkImage BImage = ResourceCast(R.texture)->Image;
                if (AImage != BImage)
                {
                    return false;
                }
            }
        }

        const RHIRenderTargetView &B = InRTInfo.ColorRenderTarget[Index];
        if (B.texture)
        {
            VkImage AImage = ColorRenderTargetImages[AttachementIndex];
            VkImage BImage = ResourceCast(B.texture)->Image;
            if (AImage != BImage)
            {
                return false;
            }
            AttachementIndex++;
        }
    }

    return true;
}

void Framebuffer::Destroy(Device &Device)
{
    VulkanRHI::DeferredDeletionQueue2 &Queue = Device.GetDeferredDeletionQueue();

    // will be deleted in reverse order
    Queue.EnqueueResource(VulkanRHI::DeferredDeletionQueue2::EType::Framebuffer, framebuffer);
    framebuffer = VK_NULL_HANDLE;
}

void Viewport::RecreateSwapchain(void *newNativeWindow)
{
    SwapChainRecreateInfo recreateInfo = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    DestroySwapchain(&recreateInfo);
    windowHandle = newNativeWindow;
    CreateSwapchain(&recreateInfo);
    check(recreateInfo.surface == VK_NULL_HANDLE);
    check(recreateInfo.swapChain == VK_NULL_HANDLE);
}

void Viewport::RecreateSwapchainFromRT(PixelFormat PreferredPixelFormat)
{
    check(IsInRenderingThread());

    // TODO: should flush RHIT commands here?

    SwapChainRecreateInfo recreateInfo = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    DestroySwapchain(&recreateInfo);
    pixelFormat = PreferredPixelFormat;
    CreateSwapchain(&recreateInfo);
    check(recreateInfo.surface == VK_NULL_HANDLE);
    check(recreateInfo.swapChain == VK_NULL_HANDLE);
}

void Viewport::Resize(uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen, PixelFormat PreferredPixelFormat)
{
    sizeX = InSizeX;
    sizeY = InSizeY;

    RecreateSwapchainFromRT(PreferredPixelFormat);
}

bool Viewport::DoCheckedSwapChainJob(std::function<int32(Viewport *)> SwapChainJob)
{
    int32 AttemptsPending = Platform::RecreateSwapchainOnFail() ? 4 : 0;
    int32 Status = SwapChainJob(this);

    while (Status < 0 && AttemptsPending > 0)
    {
        if (Status == (int32)SwapChain::EStatus::OutOfDate)
        {
            printf("Swapchain is out of date! Trying to recreate the swapchain.\n");
        }
        else if (Status == (int32)SwapChain::EStatus::SurfaceLost)
        {
            printf("Swapchain surface lost! Trying to recreate the swapchain.");
        }
        else
        {
            check(0);
        }

        RecreateSwapchain(windowHandle);

        // Swapchain creation pushes some commands - flush the command buffers now to begin with a fresh state
        device->SubmitCommandsAndFlushGPU();
        device->WaitUntilIdle();

        Status = SwapChainJob(this);

        --AttemptsPending;
    }

    return Status >= 0;
}

bool Viewport::SupportsStandardSwapchain() { return !bRenderOffscreen; }

bool Viewport::RequiresRenderingBackBuffer() { return true; }

PixelFormat Viewport::GetPixelFormatForNonDefaultSwapchain()
{
    if (bRenderOffscreen)
    {
        return PixelFormat::PF_B8G8R8A8;
    }
    else
    {
        return PixelFormat::PF_Unknown;
    }
}