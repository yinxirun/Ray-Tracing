#pragma once
#include "vulkan_memory.h"
#include "gpu/RHI/RHIDefinitions.h"
#include <iostream>

class CommandListContext;

// All the current gfx pipeline states in use
class PendingGfxState : public VulkanRHI::DeviceChild
{
public:
    PendingGfxState(Device *InDevice, CommandListContext &InContext)
        : VulkanRHI::DeviceChild(InDevice), context(InContext)
    {
        Reset();
    }

    ~PendingGfxState();

    void Reset()
    {
        viewports.resize(1, {});
        scissors.resize(1, {});
        StencilRef = 0;
        bScissorEnable = false;

        // CurrentPipeline = nullptr;
        // CurrentState = nullptr;
        // bDirtyVertexStreams = true;

        primitiveType = PT_Num;

        // //#todo-rco: Would this cause issues?
        // //FMemory::Memzero(PendingStreams);
    }

    void SetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ)
    {
        viewports.resize(1, {});

        viewports[0].x = MinX;
        viewports[0].y = MinY;
        viewports[0].width = MaxX - MinX;
        viewports[0].height = MaxY - MinY;
        viewports[0].minDepth = MinZ;
        if (MinZ == MaxZ)
        {
            // Engine pases in some cases MaxZ as 0.0
            viewports[0].maxDepth = MinZ + 1.0f;
        }
        else
        {
            viewports[0].maxDepth = MaxZ;
        }

        SetScissorRect((uint32)MinX, (uint32)MinY, (uint32)(MaxX - MinX), (uint32)(MaxY - MinY));
        bScissorEnable = false;
    }

    inline void SetScissorRect(uint32 MinX, uint32 MinY, uint32 Width, uint32 Height)
    {
        scissors.resize(1, {});

        scissors[0].offset.x = MinX;
        scissors[0].offset.y = MinY;
        scissors[0].extent.width = Width;
        scissors[0].extent.height = Height;
    }

protected:
    std::vector<VkViewport> viewports;
    std::vector<VkRect2D> scissors;

    PrimitiveType primitiveType = PT_Num;
    uint32 StencilRef;
    bool bScissorEnable;

    CommandListContext &context;
    friend class CommandListContext;
};