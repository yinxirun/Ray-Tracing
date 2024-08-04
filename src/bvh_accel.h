#pragma once

#include <memory>
#include <vector>

#include "geometry/aabb.h"
#include "geometry/ray.h"
#include "geometry/primitive.h"
#include "glm/glm.hpp"

class Material;

enum class SplitMethod { SAH, HLBVH, Middle, EqualCounts };

struct SurfaceInteraction {
  glm::vec3 normal;
  const Material* material;
  size_t hitID;

  float time = FLT_MAX;
};

class BVHBuildNode {
 public:
  AABB bounds;

  BVHBuildNode* children[2];

  int splitAxis, firstPrimOffset, nPrimitives;

  void InitLeaf(int first, int n, const AABB& b);

  void InitInterior(int axis, BVHBuildNode* c0, BVHBuildNode* c1);

  int id;
};

class BVHAccel {
 public:
  struct BVHPrimitiveInfo {
    BVHPrimitiveInfo() = default;

    BVHPrimitiveInfo(size_t primitiveNumber, const AABB& bounds);
    // Primitive ID
    size_t primitiveNumber = -1;
    AABB bounds;
    glm::vec3 centroid;
  };

  BVHAccel() = default;

  BVHAccel(std::vector<std::shared_ptr<Primitive>>& p, int maxPrimsInNode,
           SplitMethod splitMethod);

  BVHBuildNode* HLBVHBuild();

  BVHBuildNode* recursiveBuild(
      std::vector<BVHPrimitiveInfo>& primitiveInfo, int start, int end,
      int* totalNodes, std::vector<std::shared_ptr<Primitive>>& orderedPrims);

  bool Intersect(const Ray& ray, SurfaceInteraction* isect);

  std::vector<Primitive*> getAllLights();

 private:
  const int maxPrimsInNode;
  const SplitMethod splitMethod;
  std::vector<std::shared_ptr<Primitive>>& primitives;

  std::vector<BVHBuildNode*> nodes;
};