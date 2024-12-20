#pragma once

#include <memory>
#include <vector>
#include <atomic>

#include "geometry/aabb.h"
#include "geometry/ray.h"
#include "geometry/primitive.h"
#include "glm/glm.hpp"

class Material;

enum class SplitMethod
{
  SAH,
  HLBVH,
  Middle,
  EqualCounts
};

struct SurfaceInteraction
{
  glm::vec3 normal;
  const Material *material;
  size_t hitID;

  float time = FLT_MAX;
};

class BVHBuildNode
{
public:
  AABB bounds;

  BVHBuildNode *children[2];

  int splitAxis, firstPrimOffset, nPrimitives;

  void InitLeaf(int first, int n, const AABB &b);

  void InitInterior(int axis, BVHBuildNode *c0, BVHBuildNode *c1);

  int id;
};

class BVHAccel
{
public:
  struct BVHPrimitiveInfo
  {
    BVHPrimitiveInfo() = default;

    BVHPrimitiveInfo(size_t primitiveNumber, const AABB &bounds);
    // Primitive ID
    size_t primitiveNumber = -1;
    AABB bounds;
    glm::vec3 centroid;
  };

  BVHAccel() = default;

  BVHAccel(std::vector<std::shared_ptr<Primitive>> &p, int maxPrimsInNode,
           SplitMethod splitMethod);

  BVHBuildNode *HLBVHBuild(std::vector<BVHPrimitiveInfo> &primitiveInfo, int *totalNodes, std::vector<std::shared_ptr<Primitive>> &orderedPrims);

  BVHBuildNode *recursiveBuild(
      std::vector<BVHPrimitiveInfo> &primitiveInfo, int start, int end,
      int *totalNodes, std::vector<std::shared_ptr<Primitive>> &orderedPrims);

  bool Intersect(const Ray &ray, SurfaceInteraction *isect);

  std::vector<Primitive *> getAllLights();

private:
  const int maxPrimsInNode;
  const SplitMethod splitMethod;
  std::vector<std::shared_ptr<Primitive>> &primitives;

  std::vector<BVHBuildNode *> nodes;

  struct MortonPrimitive
  {
    int primitiveIndex;
    uint32_t mortonCode;
  };

  struct LBVHTreelet
  {
    int startIndex;
    int nPrimitives;
    BVHBuildNode *buildNodes;
  };

  inline uint32_t LeftShift3(uint32_t x)
  {
    if (x == (1 << 10))
      --x;
    x = (x | (x << 16)) & 0b00000011000000000000000011111111;
    x = (x | (x << 8)) & 0b00000011000000001111000000001111;
    x = (x | (x << 4)) & 0b00000011000011000011000011000011;
    x = (x | (x << 2)) & 0b00001001001001001001001001001001;
    return x;
  }

  inline uint32_t EncodeMorton3(const glm::vec3 &v)
  {
    return (LeftShift3(v.z) << 2) | (LeftShift3(v.y) << 1) | LeftShift3(v.x);
  }

  static void RadixSort(std::vector<MortonPrimitive> *v);

  /// Morton Code 一共30位。前12位相同的元素被归为一个Treelet。因此一个Treelet最多被划分18次。
  inline BVHBuildNode *emitLBVH(BVHBuildNode *&buildNodes, std::vector<BVHPrimitiveInfo> &primitiveInfo, MortonPrimitive *mortonPrims, int nPrimitives, int *totalNodes,
                                std::vector<std::shared_ptr<Primitive>> &orderedPrims, std::atomic<int> *orderedPrimsOffset, int bitIndex) const
  {
    if (bitIndex == -1 || nPrimitives < maxPrimsInNode)
    {
      // Create and return leaf node of LBVH treelet
      (*totalNodes)++;
      BVHBuildNode *node = buildNodes++;
      AABB bounds;
      int firstPrimOffset = orderedPrimsOffset->fetch_add(nPrimitives);
      for (int i = 0; i < nPrimitives; ++i)
      {
        int primitiveIndex = mortonPrims[i].primitiveIndex;
        orderedPrims[firstPrimOffset + i] = primitives[primitiveIndex];
        bounds = Union(bounds, primitiveInfo[primitiveIndex].bounds);
      }
      node->InitLeaf(firstPrimOffset, nPrimitives, bounds);
      return node;
    }
    else
    {
      int mask = 1 << bitIndex;
      // Advance to next subtree level if there’s no LBVH split for this bit
      if ((mortonPrims[0].mortonCode & mask) == (mortonPrims[nPrimitives - 1].mortonCode & mask))
        return emitLBVH(buildNodes, primitiveInfo, mortonPrims, nPrimitives, totalNodes, orderedPrims, orderedPrimsOffset, bitIndex - 1);
      // Find LBVH split point for this dimension
      int searchStart = 0, searchEnd = nPrimitives - 1;
      while (searchStart + 1 != searchEnd)
      {
        int mid = (searchStart + searchEnd) / 2;
        if ((mortonPrims[searchStart].mortonCode & mask) == (mortonPrims[mid].mortonCode & mask))
          searchStart = mid;
        else
          searchEnd = mid;
      }
      int splitOffset = searchEnd;
      // Create and return interior LBVH node

      (*totalNodes)++;
      BVHBuildNode *node = buildNodes++;
      BVHBuildNode *lbvh[2] = {
          emitLBVH(buildNodes, primitiveInfo, mortonPrims, splitOffset,
                   totalNodes, orderedPrims, orderedPrimsOffset, bitIndex - 1),
          emitLBVH(buildNodes, primitiveInfo, &mortonPrims[splitOffset], nPrimitives - splitOffset,
                   totalNodes, orderedPrims, orderedPrimsOffset, bitIndex - 1)};
      int axis = bitIndex % 3;
      node->InitInterior(axis, lbvh[0], lbvh[1]);
      return node;
    }
  }

  inline BVHBuildNode *buildUpperSAH(std::vector<BVHBuildNode *> &treeletRoots, int start, int end, int *totalNodes) const
  {
    return nullptr;
  }
};