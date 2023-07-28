#ifndef SCENE_H
#define SCENE_H

#include <array>
#include <string>
#include <vector>

#include "glm/glm.hpp"

struct Vertex {
  glm::vec3 position;
  glm::vec2 texCoord;
  glm::vec3 normal;
};

struct Material {
  std::string name;
  glm::vec3 albedo;
  glm::vec3 ambient;
  glm::vec3 emissive;
};

class Triangle {
 public:
  Triangle() {}
  Triangle(std::array<const Vertex*, 3>& _v, const Material* _mat);
  inline glm::vec3 center() const {
    return (v[0]->position + v[1]->position + v[2]->position) / 3.0f;
  }
  inline glm::vec3 getPosition(int i) const { return v[i]->position; }
  inline glm::vec3 getNormal() const { return normal; }
  const Material* getMaterial() const;
  size_t id;

  /// @brief 返回三角形上随机一点
  glm::vec3 getRandomPoint() const;
  float getArea() const;

 private:
  std::array<const Vertex*, 3> v;  ///< 3个顶点的指针
  glm::vec3 normal;                ///< 三角形平面的法向量
  const Material* material;
};

class Mesh {
 public:
  Mesh() {}
  Mesh(std::vector<Vertex>& _vertices, std::vector<unsigned>& _indices,
       unsigned _matIds);
  // std::vector<const Triangle*> getTriangles();
  inline unsigned getNumFaces() { return indices.size() / 3; }
  inline const std::vector<Vertex>& getVertices() { return vertices; }
  inline const std::vector<unsigned>& getIndices() { return indices; }
  inline unsigned getMaterialIndex() { return materialIndex; }

 private:
  std::vector<Vertex> vertices;
  std::vector<unsigned> indices;
  // std::vector<Triangle> faces;
  unsigned materialIndex;
};

class Scene {
 public:
  void load(std::string fileName);
  inline std::vector<Mesh>& getMeshes() { return meshes; }
  std::vector<Triangle> getAllFaces();
  const std::vector<Triangle>& getAllLights() { return lights; }

 private:
  std::vector<Mesh> meshes;
  std::vector<Material> materials;
  std::vector<Triangle> lights;
};

#endif