#pragma once

#include "render_core/renderer_interface.h"

/** The renderer module implementation. */
class RendererModule final : public IRendererModule
{
public:
    RendererModule() = default;

    void BeginRenderingViewFamily(SceneViewFamily *ViewFamily);
};