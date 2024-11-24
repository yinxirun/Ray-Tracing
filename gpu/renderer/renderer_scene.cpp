#include "scene_private.h"

#include <memory>

Scene::Scene() : GPUScene(*this) {}

Scene::~Scene() {}

void Scene::AddPrimitive(std::shared_ptr<PrimitiveComponent> primitive)
{
    addedPrimitive.insert(primitive);
}

void Scene::RemovePrimitive(std::shared_ptr<PrimitiveComponent> primitive)
{
    addedPrimitive.erase(primitive);
}