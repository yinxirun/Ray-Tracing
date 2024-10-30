#include "viewport.h"
#include "rhi.h"
#include <vector>
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

BackBuffer::BackBuffer(Device &device, Viewport *InViewport, VkFormat Format, uint32_t SizeX, uint32_t SizeY, ETextureCreateFlags UEFlags)
    : Texture(device, RHITextureCreateDesc::Create2D("FVulkanBackBuffer", SizeX, SizeY, Format).SetFlags(UEFlags).DetermineInititialState(), VK_NULL_HANDLE, false),
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

Viewport::Viewport(Device *d, void *InWindowHandle, uint32_t sizeX, uint32_t sizeY, EPixelFormat InPreferredPixelFormat)
    : device(d), sizeX(sizeX), sizeY(sizeY), swapChain(nullptr), windowHandle(InWindowHandle), pixelFormat(InPreferredPixelFormat),
      acquiredSemaphore(nullptr)
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

    // RHI::Get().viewports.remove(this);
}

inline static void CopyImageToBackBuffer(CommandListContext *context, CmdBuffer *cmdBuffer,
                                         Texture &srcSurface, VkImage dstSurface, int32 sizeX, int32 sizeY,
                                         int32 windowSizeX, int32 windowSizeY)
{
    LayoutManager &LayoutManager = cmdBuffer->GetLayoutManager();
    const VkImageLayout PreviousSrcLayout = LayoutManager::SetExpectedLayout(cmdBuffer, srcSurface, ERHIAccess::CopySrc);

    {
        PipelineBarrier Barrier;
        Barrier.AddImageLayoutTransition(dstSurface,
                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         PipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1));
        Barrier.Execute(cmdBuffer);
    }

    printf("Am i really need VulkanRHI::DebugHeavyWeightBarrier?? %s %d\n", __FILE__, __LINE__);
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

void Viewport::WaitForFrameEventCompletion()
{
    // if (Platform::RequiresWaitingForFrameCompletionEvent())
    // {
    //     //		static FCriticalSection CS;
    //     //		FScopeLock ScopeLock(&CS);
    //     if (LastFrameCommandBuffer && LastFrameCommandBuffer->IsSubmitted())
    //     {
    //         // If last frame's fence hasn't been signaled already, wait for it here
    //         if (LastFrameFenceCounter == LastFrameCommandBuffer->GetFenceSignaledCounter())
    //         {
    //             if (!GWaitForIdleOnSubmit)
    //             {
    //                 // The wait has already happened if GWaitForIdleOnSubmit is set
    //                 LastFrameCommandBuffer->GetOwner()->GetMgr().WaitForCmdBuffer(LastFrameCommandBuffer);
    //             }
    //         }
    //     }
    // }
    printf("Have not implement Viewport::WaitForFrameEventCompletion\n");
}

void Viewport::IssueFrameEvent()
{
    // if (FVulkanPlatform::RequiresWaitingForFrameCompletionEvent())
    // {
    //     // The fence we need to wait on next frame is already there in the command buffer
    //     // that was just submitted in this frame's Present. Just grab that command buffer's
    //     // info to use next frame in WaitForFrameEventCompletion.
    //     Queue *queue = device->GetGraphicsQueue();
    //     queue->GetLastSubmittedInfo(LastFrameCommandBuffer, LastFrameFenceCounter);
    // }
    printf("Have not implement Viewport::WaitForFrameEventCompletion\n");
}

bool Viewport::Present(CommandListContext *context, CmdBuffer *cmdBuffer, Queue *queue, Queue *presentQueue, bool bLockToVsync)
{
    // FPlatformAtomics::AtomicStore(&LockToVsync, bLockToVsync ? 1 : 0);
    bool bFailedToDelayAcquireBackbuffer = false;

    // Transition back buffer to presentable and submit that command
    check(cmdBuffer->IsOutsideRenderPass());

    if (SupportsStandardSwapchain())
    {
        if (GVulkanDelayAcquireImage == EDelayAcquireImageType::DelayAcquire && RenderingBackBuffer)
        {
            // swapchain can go out of date, do not crash at this point
            if (TryAcquireImageIndex())
            {
                uint32 WindowSizeX = std::min(sizeX, swapChain->internalWidth);
                uint32 WindowSizeY = std::min(sizeY, swapChain->internalHeight);

                context->RHIPushEvent("CopyImageToBackBuffer", 0);
                CopyImageToBackBuffer(context, cmdBuffer, *RenderingBackBuffer.get(), backBufferImages[acquiredImageIndex],
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
                check(RHIBackBuffer != nullptr && RHIBackBuffer->Image == RenderingBackBuffer->Image);
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

            VulkanRHI::Semaphore *SignalSemaphore = (acquiredImageIndex >= 0 ? renderingDoneSemaphores[acquiredImageIndex] : nullptr);
            // submit through the CommandBufferManager as it will add the proper semaphore
            ImmediateCmdBufMgr->SubmitActiveCmdBufferFromPresent(SignalSemaphore);
        }
        else
        {
            // failing to do the delayacquire can only happen if we were in this mode to begin with
            check(GVulkanDelayAcquireImage == EDelayAcquireImageType::DelayAcquire);

            // UE_LOG(LogVulkanRHI, Log, TEXT("AcquireNextImage() failed due to the outdated swapchain, not even attempting to present."));

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

    // Flush all commands
    // check(0);

    // #todo-rco: Proper SyncInterval bLockToVsync ? RHIConsoleVariables::SyncInterval : 0
    int32 SyncInterval = 0;
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

    ++presentCount;

    return bResult;
}

void Viewport::CreateSwapchain(struct SwapChainRecreateInfo *recreateInfo)
{
    if (SupportsStandardSwapchain())
    {
        uint32_t desiredNumBackBuffers = NUM_BUFFERS;

        std::vector<VkImage> Images;

        if (pixelFormat == EPixelFormat::PF_B8G8R8A8)
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

        RHIBackBuffer = std::make_shared<BackBuffer>(*device, this, UEToVkTextureFormat(pixelFormat, false), sizeX, sizeY, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_ResolveTargetable);
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
}

void Viewport::DestroySwapchain(struct SwapChainRecreateInfo *RecreateInfo)
{
    // Submit all command buffers here
    device->SubmitCommandsAndFlushGPU();
    device->WaitUntilIdle();

    // Intentionally leave RenderingBackBuffer alive, so it can be used a dummy backbuffer while we don't have swapchain images
    // RenderingBackBuffer = nullptr;

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

void Viewport::RecreateSwapchain(void *NewNativeWindow)
{
    // FScopeLock LockSwapchain(&RecreatingSwapchain);

    SwapChainRecreateInfo RecreateInfo = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    DestroySwapchain(&RecreateInfo);
    windowHandle = NewNativeWindow;
    CreateSwapchain(&RecreateInfo);
    check(RecreateInfo.surface == VK_NULL_HANDLE);
    check(RecreateInfo.swapChain == VK_NULL_HANDLE);
}

bool Viewport::DoCheckedSwapChainJob(std::function<int32(Viewport *)> SwapChainJob)
{
    printf("Have not implement Viewport::DoCheckedSwapChainJob\n");
    return true;
}

bool Viewport::SupportsStandardSwapchain() { return !bRenderOffscreen; }

EPixelFormat Viewport::GetPixelFormatForNonDefaultSwapchain()
{
    if (bRenderOffscreen)
    {
        return EPixelFormat::PF_B8G8R8A8;
    }
    else
    {
        return EPixelFormat::PF_Unknown;
    }
}