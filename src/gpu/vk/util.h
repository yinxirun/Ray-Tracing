#pragma once

#include "Volk/volk.h"
#include "resources.h"

namespace VulkanRHI
{
    /**
     * Checks that the given result isn't a failure.  If it is, the application exits with an appropriate error message.
     * @param	Result - The result code to check
     * @param	Code - The code which yielded the result.
     * @param	VkFunction - Tested function name.
     * @param	Filename - The filename of the source file containing Code.
     * @param	Line - The line number of Code within Filename.
     */
    extern void VerifyVulkanResult(VkResult Result, const char *VkFuntion, const char *Filename, uint32_t Line);
}

#define VERIFYVULKANRESULT(VkFunction)                                                    \
    {                                                                                     \
        const VkResult ScopedResult = VkFunction;                                         \
        if (ScopedResult != VK_SUCCESS)                                                   \
        {                                                                                 \
            VulkanRHI::VerifyVulkanResult(ScopedResult, #VkFunction, __FILE__, __LINE__); \
        }                                                                                 \
    }

#define VERIFYVULKANRESULT_EXPANDED(VkFunction)                                           \
    {                                                                                     \
        const VkResult ScopedResult = VkFunction;                                         \
        if (ScopedResult < VK_SUCCESS)                                                    \
        {                                                                                 \
            VulkanRHI::VerifyVulkanResult(ScopedResult, #VkFunction, __FILE__, __LINE__); \
        }                                                                                 \
    }
