#pragma once

#include "Volk/volk.h"
#include "device.h"

class Device;
class CommandListContext;
class CommandBufferManager;

class CmdBuffer
{
public:
    CmdBuffer(Device *InDevice, CommandBufferPool *InCommandBufferPool) : Device(InDevice), CommandBufferHandle(VK_NULL_HANDLE), CommandBufferPool(InCommandBufferPool)
    {
        AllocMemory();
    }
    void Begin();
    void End();

private:
    Device *Device;
    VkCommandBuffer CommandBufferHandle;
    CommandBufferPool *CommandBufferPool;

    void AllocMemory()
    {
        VkCommandBufferAllocateInfo CreateCmdBufInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        CreateCmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        CreateCmdBufInfo.commandBufferCount = 1;
        CreateCmdBufInfo.commandPool = CommandBufferPool->GetHandle();

        vkAllocateCommandBuffers(Device->GetInstanceHandle(), &CreateCmdBufInfo, &CommandBufferHandle);
    }

    void FreeMemory()
    {
        vkFreeCommandBuffers(Device->GetInstanceHandle(), CommandBufferPool->GetHandle(), 1, &CommandBufferHandle);
        CommandBufferHandle = VK_NULL_HANDLE;
    }
};

class CommandBufferPool
{
public:
    CommandBufferPool(Device *device, CommandBufferManager &mgr) : Handle(VK_NULL_HANDLE), Device(device), Mgr(mgr) {}

    VkCommandPool GetHandle() const { return Handle; }

private:
    VkCommandPool Handle;
    Device *Device;
    CommandBufferManager &Mgr;
};

class CommandBufferManager
{
public:
    CommandBufferManager(Device *InDevice, CommandListContext *InContext)
    {
    }
};