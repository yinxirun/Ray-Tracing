#include "util.h"
#include <iostream>

namespace VulkanRHI
{
    void VerifyVulkanResult(VkResult Result, const char *VkFunction, const char *Filename, uint32_t Line)
    {
        printf("VerifyVulkanResult %s %s %u\n", VkFunction, Filename, Line);
    }
}