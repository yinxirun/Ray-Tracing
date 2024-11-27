#pragma once

#include <memory>
#include "RHI/RHIContext.h"

/** RDG struct containing the complete set of scene textures for the deferred or mobile renderers. */
struct SceneTextures
{
    std::shared_ptr<Texture> GBufferA; // Base Color
    std::shared_ptr<Texture> GBufferB; // Normal
    std::shared_ptr<Texture> GBufferC; // position
    std::shared_ptr<Texture> GBufferD; // roughness-metallic

    std::shared_ptr<Texture> Depth;
};