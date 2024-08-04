#pragma once

#include "glm/glm.hpp"

class Ray;

struct AABB {
  glm::vec3 pMin = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
  glm::vec3 pMax = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

  int MaximumExtent();

  glm::vec3 Offset(const glm::vec3& p) const;

  float SurfaceArea();

  bool Intersect(const Ray& ray) const;
};

AABB Union(AABB& u, AABB& v);

AABB Union(AABB& u, glm::vec3& v);