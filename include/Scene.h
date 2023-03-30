#ifndef _SCENE_H
#define _SCENE_H
#include <string>
#include <vector>

#include "Material.h"
#include "Mesh.h"

class Scene {
 public:
  Mesh* mesh;
  std::vector<Material*> materials;
  std::vector<AreaLight*> light_sources;

  Scene() { mesh = new Mesh(); }
  ~Scene();

  /// @return Index of the matrial named "name".
  Material* find_material(std::string name);
};
#endif