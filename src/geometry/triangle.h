#pragma once

#include "primitive.h"
#include "glm/glm.hpp"
#include "vertex.h"
#include <array>

class Material;

class Triangle : public Primitive {
 public:
  Triangle() = default;

  Triangle(std::array<const Vertex *, 3> &_v, const Material *_mat);

  glm::vec3 Center() const;

  glm::vec3 GetNormal() const;

  size_t GetID() const;

  void SetID(size_t id);

  const Material *GetMaterial() const;

  glm::vec3 RandomPoint() const;

  float Area() const;

  AABB WorldBound() override;

  bool Intersect(const Ray &, SurfaceInteraction *) override;

  bool Emissive() override;

 private:
  std::array<const Vertex *, 3> v;

  glm::vec3 normal;

  const Material *material;

  size_t id;
};