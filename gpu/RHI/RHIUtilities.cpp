#include "RHIUtilities.h"

Access RHIGetDefaultResourceState(ETextureCreateFlags InUsage, bool bInHasInitialData)
{
    // By default assume it can be bound for reading
    Access ResourceState = Access::SRVMask;

    auto EnumHasAnyFlags = [](ETextureCreateFlags usage, ETextureCreateFlags flags) -> bool
    {
        uint64 u = (uint64)usage;
        uint64 f = (uint64)flags;
        return (u & f) != 0;
    };

    if (!bInHasInitialData)
    {
        if (EnumHasAnyFlags(InUsage, TexCreate_RenderTargetable))
        {
            ResourceState = Access::RTV;
        }
        else if (EnumHasAnyFlags(InUsage, TexCreate_DepthStencilTargetable))
        {
            ResourceState = Access::DSVWrite | Access::DSVRead;
        }
        else if (EnumHasAnyFlags(InUsage, TexCreate_UAV))
        {
            ResourceState = Access::UAVMask;
        }
        else if (EnumHasAnyFlags(InUsage, TexCreate_Presentable))
        {
            ResourceState = Access::Present;
        }
        else if (EnumHasAnyFlags(InUsage, TexCreate_ShaderResource))
        {
            ResourceState = Access::SRVMask;
        }
        else if (EnumHasAnyFlags(InUsage, TexCreate_Foveation))
        {
            ResourceState = Access::ShadingRateSource;
        }
    }

    return ResourceState;
}