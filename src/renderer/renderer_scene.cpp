#include "scene_private.h"

#include <memory>

Scene::Scene() : GPUScene(*this) {}

Scene::~Scene() {}

void Scene::AddPrimitive(std::shared_ptr<PrimitiveComponent> primitive)
{
    primitives.push_back(primitive);
}

void Scene::RemovePrimitive(std::shared_ptr<PrimitiveComponent> primitive)
{
    check(0);
}