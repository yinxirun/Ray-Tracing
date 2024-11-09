#include "dynamic_rhi.h"

RHI *rhi = nullptr;

void RHIInit()
{
    rhi = &RHI::Get();
    rhi->Init();
}

void RHIExit()
{
    rhi->Shutdown();
}