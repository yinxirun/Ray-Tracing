#include "descriptor_sets.h"
#include "private.h"
#include "device.h"
#include "renderpass.h"
#include "context.h"
#include "pending_state.h"


// Whether to use VK_ACCESS_SHADER_READ_BIT an input attachments to workaround rendering issues
// 0 use: VK_ACCESS_INPUT_ATTACHMENT_READ_BIT (default)
// 1 use: VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT
int32 GVulkanInputAttachmentShaderRead = 0;

// 461
void CommandListContext::ReleasePendingState()
{
    delete pendingGfxState;
    pendingGfxState = nullptr;
}

// 1640
DescriptorSetsLayout::DescriptorSetsLayout(Device *InDevice) : device(InDevice) {}

DescriptorSetsLayout::~DescriptorSetsLayout()
{
    // Handles are owned by FVulkanPipelineStateCacheManager
    layoutHandles.resize(0);
}

// 1885
RenderPass::RenderPass(Device &InDevice, const RenderTargetLayout &InRTLayout)
    : Layout(InRTLayout),
      renderPass(VK_NULL_HANDLE),
      NumUsedClearValues(InRTLayout.GetNumUsedClearValues()),
      device(InDevice)
{
    renderPass = CreateVulkanRenderPass(InDevice, InRTLayout);
}

RenderPass::~RenderPass()
{
    device.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::DeferredDeletionQueue2::EType::RenderPass, renderPass);
    renderPass = VK_NULL_HANDLE;
}