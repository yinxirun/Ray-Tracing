#pragma once
class RHICommandListImmediate;

class DynamicRHI
{
public:
    /** Declare a virtual destructor, so the dynamic RHI can be deleted without knowing its type. */
    virtual ~DynamicRHI() {}

    /** Initializes the RHI; separate from IDynamicRHIModule::CreateRHI so that GDynamicRHI is set when it is called. */
    virtual void Init() = 0;

    /** Shutdown the RHI; handle shutdown and resource destruction before the RHI's actual destructor is called (so that all resources of the RHI are still available for shutdown). */
    virtual void Shutdown() = 0;

    /////// RHI Methods

    virtual void BeginFrame(RHICommandListImmediate &RHICmdList) = 0;
};