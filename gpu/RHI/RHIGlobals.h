#include "gpu/definitions.h"
#include "gpu/RHI/RHIDefinitions.h"

struct RHIGlobals
{
    /** Whether or not the RHI can handle a non-zero FirstInstance to DrawIndexedPrimitive and friends - extra SetStreamSource calls will be needed if this is false */
    bool SupportsFirstInstance = false;

    struct FVariableRateShading
    {
        /** Whether VRS (in all flavors) is currently enabled (separate from whether it's supported/available). */
        bool Enabled = true;

        /** Whether or not the RHI can support per-draw Variable Rate Shading. */
        bool SupportsPipeline = false;

        /** Data type contained in a shading-rate image for image-based Variable Rate Shading. */
        VRSImageDataType ImageDataType = VRSImage_NotSupported;

    } VariableRateShading;
};
extern RHIGlobals GRHIGlobals;

#define GRHISupportsFirstInstance GRHIGlobals.SupportsFirstInstance

#define GRHISupportsPipelineVariableRateShading GRHIGlobals.VariableRateShading.SupportsPipeline
#define GRHIVariableRateShadingEnabled GRHIGlobals.VariableRateShading.Enabled
#define GRHIVariableRateShadingImageDataType GRHIGlobals.VariableRateShading.ImageDataType