#include"Scene.h"

//---------------------------------------------------------------------------------
Scene::~Scene() {
  delete mesh;
  for (Material* i : materials) delete i;
  for (AreaLight* i : light_sources) delete i;
}

Material* Scene::find_material(std::string name) {
  for (int i = 0; i < materials.size(); i++)
    if (materials[i]->name.compare(name) == 0) return materials[i];
  return nullptr;
}
