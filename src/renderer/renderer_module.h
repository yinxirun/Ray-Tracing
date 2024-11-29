#pragma once
#include "RHI/RHIResources.h"
#include "render_core/renderer_interface.h"
#include <array>
#include <memory>

/** The renderer module implementation. */
class RendererModule final : public IRendererModule
{
public:
    RendererModule() = default;

    void BeginRenderingViewFamily(Camera *ViewFamily);
};