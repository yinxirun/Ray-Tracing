#pragma once

#include "simple_scene.h"
#include "RHI/RHICommandList.h"

class SimpleStaticMesh;
class CommandContext;
class SimpleScene;

class SimpleRenderer
{
public:
    SimpleRenderer(SimpleScene &s);
    void Render(CommandContext *context, SimpleScene &scene, Viewport* viewport);

    void Prepare(RHICommandListBase &immediate);

private:
    SimpleScene &scene;
    std::shared_ptr<UniformBuffer> ub;
    std::shared_ptr<Texture> depth;
};