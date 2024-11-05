#include "util.h"
#include "command_buffer.h"
#include <iostream>

namespace VulkanRHI
{
    void VerifyVulkanResult(VkResult Result, const char *VkFunction, const char *Filename, uint32_t Line)
    {
        printf("VerifyVulkanResult %s %s %u\n", VkFunction, Filename, Line);
    }
}


void VulkanGPUFence::Clear()
{
    CmdBuffer = nullptr;
    FenceSignaledCounter = UINT64_MAX;
}

bool VulkanGPUFence::Poll() const
{
    return (CmdBuffer && (FenceSignaledCounter < CmdBuffer->GetFenceSignaledCounter()));
}