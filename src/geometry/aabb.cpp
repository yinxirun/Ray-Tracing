#include "aabb.h"
#include "../global.h"
#include "ray.h"
#include <array>

AABB Union(AABB& u, AABB& v) {
  float xmin = std::min(u.pMin.x, v.pMin.x);
  float ymin = std::min(u.pMin.y, v.pMin.y);
  float zmin = std::min(u.pMin.z, v.pMin.z);
  float xmax = std::max(u.pMax.x, v.pMax.x);
  float ymax = std::max(u.pMax.y, v.pMax.y);
  float zmax = std::max(u.pMax.z, v.pMax.z);

  AABB bound;
  bound.pMin = glm::vec3(xmin, ymin, zmin);
  bound.pMax = glm::vec3(xmax, ymax, zmax);
  return bound;
}

AABB Union(AABB& u, glm::vec3& v) {
  float xmin = std::min(u.pMin.x, v.x);
  float ymin = std::min(u.pMin.y, v.y);
  float zmin = std::min(u.pMin.z, v.z);
  float xmax = std::max(u.pMax.x, v.x);
  float ymax = std::max(u.pMax.y, v.y);
  float zmax = std::max(u.pMax.z, v.z);

  AABB bound;
  bound.pMin = glm::vec3(xmin, ymin, zmin);
  bound.pMax = glm::vec3(xmax, ymax, zmax);
  return bound;
}

int AABB::MaximumExtent() {
  std::array<float, 3> v;
  v[0] = pMax.x - pMin.x;
  v[1] = pMax.y - pMin.y;
  v[2] = pMax.z - pMax.z;
  float max = -FLT_MAX;
  int max_extent;
  for (int i = 0; i < 3; ++i) {
    if (v[i] > max) {
      max = v[i];
      max_extent = i;
    }
  }
  return max_extent;
}

glm::vec3 AABB::Offset(const glm::vec3& p) const {
  glm::vec3 o = glm::vec3(p[0], p[1], p[2]) - pMin;
  if (pMax.x > pMin.x) o.x /= pMax.x - pMin.x;
  if (pMax.y > pMin.y) o.y /= pMax.y - pMin.y;
  if (pMax.z > pMin.z) o.z /= pMax.z - pMin.z;
  return o;
}

float AABB::SurfaceArea() {
  glm::vec3 diagonal = pMax - pMin;
  return 2 * (diagonal.x * diagonal.y + diagonal.x * diagonal.z +
              diagonal.y * diagonal.z);
}

bool AABB::Intersect(const Ray& ray) const {
  glm::vec3 invDir(1.0f / ray.direction.x, 1.0f / ray.direction.y,
                   1.0f / ray.direction.z);
  float txMin, txMax, tyMin, tyMax, tzMin, tzMax;
  if (ray.direction.x > 0) {
    txMin = (pMin.x - ray.origin.x) * invDir.x;
    txMax = (pMax.x - ray.origin.x) * invDir.x;
  } else {
    txMin = (pMax.x - ray.origin.x) * invDir.x;
    txMax = (pMin.x - ray.origin.x) * invDir.x;
  }
  if (ray.direction.y > 0) {
    tyMin = (pMin.y - ray.origin.y) * invDir.y;
    tyMax = (pMax.y - ray.origin.y) * invDir.y;
  } else {
    tyMin = (pMax.y - ray.origin.y) * invDir.y;
    tyMax = (pMin.y - ray.origin.y) * invDir.y;
  }
  if (ray.direction.z > 0) {
    tzMin = (pMin.z - ray.origin.z) * invDir.z;
    tzMax = (pMax.z - ray.origin.z) * invDir.z;
  } else {
    tzMin = (pMax.z - ray.origin.z) * invDir.z;
    tzMax = (pMin.z - ray.origin.z) * invDir.z;
  }

  // ensure robust bounds intersection
  // txMax *= 1 + 2 * gamma(3);
  // tyMax *= 1 + 2 * gamma(3);
  // tzMax *= 1 + 2 * gamma(3);

  float tMin = std::max(txMin, std::max(tyMin, tzMin));
  float tMax = std::min(txMax, std::min(tyMax, tzMax));

  return tMin < tMax + epsilon && tMin < ray.tMax && tMax > 0;
}
