#pragma once

class Scene;

class GPUScene
{
public:
    GPUScene(Scene &scene);

private:
    Scene &scene;
};