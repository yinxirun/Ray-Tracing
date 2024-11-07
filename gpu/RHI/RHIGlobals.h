struct RHIGlobals
{
    /** Whether or not the RHI can handle a non-zero FirstInstance to DrawIndexedPrimitive and friends - extra SetStreamSource calls will be needed if this is false */
    bool SupportsFirstInstance = false;
};
extern RHIGlobals GRHIGlobals;

#define GRHISupportsFirstInstance GRHIGlobals.SupportsFirstInstance