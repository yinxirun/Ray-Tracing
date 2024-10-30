#include "RHIUtilities.h"

ERHIAccess RHIGetDefaultResourceState(ETextureCreateFlags InUsage, bool bInHasInitialData)
{
    // By default assume it can be bound for reading
    ERHIAccess ResourceState = ERHIAccess::SRVMask;

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
            ResourceState = ERHIAccess::RTV;
        }
        else if (EnumHasAnyFlags(InUsage, TexCreate_DepthStencilTargetable))
        {
            ResourceState = ERHIAccess::DSVWrite | ERHIAccess::DSVRead;
        }
        else if (EnumHasAnyFlags(InUsage, TexCreate_UAV))
        {
            ResourceState = ERHIAccess::UAVMask;
        }
        else if (EnumHasAnyFlags(InUsage, TexCreate_Presentable))
        {
            ResourceState = ERHIAccess::Present;
        }
        else if (EnumHasAnyFlags(InUsage, TexCreate_ShaderResource))
        {
            ResourceState = ERHIAccess::SRVMask;
        }
        else if (EnumHasAnyFlags(InUsage, TexCreate_Foveation))
        {
            ResourceState = ERHIAccess::ShadingRateSource;
        }
    }

    return ResourceState;
}