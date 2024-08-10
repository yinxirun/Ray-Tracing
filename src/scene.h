#ifndef SCENE_H
#define SCENE_H

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "bvh_accel.h"
#include "geometry/triangle.h"
#include "geometry/vertex.h"
#include "glm/glm.hpp"

enum class MaterialType { DIFFUSE, SPECULAR, EMISSIVE };

struct Material {
  std::string name;
  glm::vec3 albedo;
  glm::vec3 emission;
  float roughness = 0;
  float metallic = 0;
  MaterialType type;
};

class Section {
 public:
  std::vector<unsigned> polygonIDs;
  unsigned materialId = 0;
};

class Mesh {
 public:
  Mesh() = default;
  inline unsigned getNumFaces() { return indices.size() / 3; }
  inline const std::vector<Vertex> &getVertices() { return vertices; }
  inline const std::vector<unsigned> &getIndices() { return indices; }

  std::vector<Section> sections;
  std::vector<Vertex> vertices;
  std::vector<unsigned> indices;
};

class Scene {
 public:
  std::vector<std::shared_ptr<Primitive>> AllTriangles();
  void load(std::string fileName);

 private:
  std::vector<Mesh> meshes;
  std::vector<std::unique_ptr<Material>> materials;
};

#endif