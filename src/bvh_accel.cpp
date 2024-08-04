#include "global.h"
#include "bvh_accel.h"

#include <fstream>
#include <algorithm>

#include "path_tracing.h"
#include "scene.h"
#include "geometry/aabb.h"
#include "geometry/primitive.h"

extern std::fstream recorder;

void BVHBuildNode::InitLeaf(int first, int n, const AABB& b) {
  firstPrimOffset = first;
  nPrimitives = n;
  bounds = b;
  children[0] = children[1] = nullptr;
}

void BVHBuildNode::InitInterior(int axis, BVHBuildNode* c0, BVHBuildNode* c1) {
  children[0] = c0;
  children[1] = c1;
  bounds = Union(c0->bounds, c1->bounds);
  splitAxis = axis;
  nPrimitives = 0;
}

BVHAccel::BVHPrimitiveInfo::BVHPrimitiveInfo(size_t primitiveNumber,
                                             const AABB& bounds)
    : primitiveNumber(primitiveNumber),
      bounds(bounds),
      centroid(.5f * bounds.pMin + .5f * bounds.pMax) {}

BVHAccel::BVHAccel(std::vector<std::shared_ptr<Primitive>>& p,
                   int maxPrimsInNode, SplitMethod splitMethod)
    : maxPrimsInNode(std::min(255, maxPrimsInNode)),
      primitives(p),
      splitMethod(splitMethod) {
  if (primitives.size() == 0) return;

  std::vector<BVHPrimitiveInfo> primitiveInfo(primitives.size());
  for (size_t i = 0; i < primitives.size(); ++i)
    primitiveInfo[i] = {i, primitives[i]->WorldBound()};

  int totalNodes = 0;
  std::vector<std::shared_ptr<Primitive>> orderedPrims;
  BVHBuildNode* root;
  if (splitMethod == SplitMethod::HLBVH)
    ;
  //     root = HLBVHBuild(arena, primitiveInfo, &totalNodes, orderedPrims);
  else
    root = recursiveBuild(primitiveInfo, 0, primitives.size(), &totalNodes,
                          orderedPrims);
  primitives.swap(orderedPrims);
}

struct BucketInfo {
  int count = 0;
  AABB bounds;
};

BVHBuildNode* BVHAccel::recursiveBuild(
    std::vector<BVHPrimitiveInfo>& primitiveInfo, int start, int end,
    int* totalNodes, std::vector<std::shared_ptr<Primitive>>& orderedPrims) {
  BVHBuildNode* node = new BVHBuildNode;
  nodes.push_back(node);
  (*totalNodes)++;

  // Compute bounds of all primitives in BVH node
  AABB bounds;
  for (int i = start; i < end; ++i)
    bounds = Union(bounds, primitiveInfo[i].bounds);

  int nPrimitives = end - start;
  if (nPrimitives == 1) {
    // Create leaf _BVHBuildNode_
    int firstPrimOffset = orderedPrims.size();
    for (int i = start; i < end; ++i) {
      int primNum = primitiveInfo[i].primitiveNumber;
      orderedPrims.push_back(primitives[primNum]);
    }
    node->InitLeaf(firstPrimOffset, nPrimitives, bounds);
    return node;
  } else {
    // Compute bound of primitive centroids, choose split dimension _dim_
    AABB centroidBounds;
    for (int i = start; i < end; ++i)
      centroidBounds = Union(centroidBounds, primitiveInfo[i].centroid);

    int dim = centroidBounds.MaximumExtent();

    // Partition primitives into two sets and build children
    int mid = (start + end) / 2;
    if (centroidBounds.pMax[dim] == centroidBounds.pMin[dim]) {
      // Create leaf _BVHBuildNode_
      int firstPrimOffset = orderedPrims.size();
      for (int i = start; i < end; ++i) {
        int primNum = primitiveInfo[i].primitiveNumber;
        orderedPrims.push_back(primitives[primNum]);
      }
      node->InitLeaf(firstPrimOffset, nPrimitives, bounds);
      return node;
    } else {
      // Partition primitives based on _splitMethod_
      switch (splitMethod) {
        case SplitMethod::Middle: {
          // Partition primitives through node's midpoint
          float pmid =
              (centroidBounds.pMin[dim] + centroidBounds.pMax[dim]) / 2;
          BVHPrimitiveInfo* midPtr =
              std::partition(&primitiveInfo[start], &primitiveInfo[end - 1] + 1,
                             [dim, pmid](const BVHPrimitiveInfo& pi) {
                               return pi.centroid[dim] < pmid;
                             });
          mid = midPtr - &primitiveInfo[0];
          // For lots of prims with large overlapping bounding boxes, this
          // may fail to partition; in that case don't break and fall
          // through
          // to EqualCounts.
          if (mid != start && mid != end) break;
        }
        case SplitMethod::EqualCounts: {
          // Partition primitives into equally-sized subsets
          mid = (start + end) / 2;
          std::nth_element(
              &primitiveInfo[start], &primitiveInfo[mid],
              &primitiveInfo[end - 1] + 1,
              [dim](const BVHPrimitiveInfo& a, const BVHPrimitiveInfo& b) {
                return a.centroid[dim] < b.centroid[dim];
              });
          break;
        }
        case SplitMethod::SAH:
        default: {
          // Partition primitives using approximate SAH
          if (nPrimitives <= 2) {
            // Partition primitives into equally-sized subsets
            mid = (start + end) / 2;
            std::nth_element(
                &primitiveInfo[start], &primitiveInfo[mid],
                &primitiveInfo[end - 1] + 1,
                [dim](const BVHPrimitiveInfo& a, const BVHPrimitiveInfo& b) {
                  return a.centroid[dim] < b.centroid[dim];
                });
          } else {
            // Allocate _BucketInfo_ for SAH partition buckets
            constexpr int nBuckets = 12;
            BucketInfo buckets[nBuckets];

            // Initialize _BucketInfo_ for SAH partition buckets
            for (int i = start; i < end; ++i) {
              int b = nBuckets *
                      centroidBounds.Offset(primitiveInfo[i].centroid)[dim];
              if (b == nBuckets) b = nBuckets - 1;
              b = b >= 0 ? b : 0;
              b = b < nBuckets ? b : nBuckets - 1;
              buckets[b].count++;
              buckets[b].bounds =
                  Union(buckets[b].bounds, primitiveInfo[i].bounds);
            }

            // Compute costs for splitting after each bucket
            float cost[nBuckets - 1];
            for (int i = 0; i < nBuckets - 1; ++i) {
              AABB b0, b1;
              int count0 = 0, count1 = 0;
              for (int j = 0; j <= i; ++j) {
                b0 = Union(b0, buckets[j].bounds);
                count0 += buckets[j].count;
              }
              for (int j = i + 1; j < nBuckets; ++j) {
                b1 = Union(b1, buckets[j].bounds);
                count1 += buckets[j].count;
              }
              cost[i] =
                  1 + (count0 * b0.SurfaceArea() + count1 * b1.SurfaceArea()) /
                          bounds.SurfaceArea();
            }

            // Find bucket to split at that minimizes SAH metric
            float minCost = cost[0];
            int minCostSplitBucket = 0;
            for (int i = 1; i < nBuckets - 1; ++i) {
              if (cost[i] < minCost) {
                minCost = cost[i];
                minCostSplitBucket = i;
              }
            }

            // Either create leaf or split primitives at selected SAH
            // bucket
            float leafCost = nPrimitives;
            if (nPrimitives > maxPrimsInNode || minCost < leafCost) {
              BVHPrimitiveInfo* pmid = std::partition(
                  &primitiveInfo[start], &primitiveInfo[end - 1] + 1,
                  [=](const BVHPrimitiveInfo& pi) {
                    int b = nBuckets * centroidBounds.Offset(pi.centroid)[dim];
                    if (b == nBuckets) b = nBuckets - 1;
                    b = b >= 0 ? b : 0;
                    b = b < nBuckets ? b : nBuckets - 1;
                    return b <= minCostSplitBucket;
                  });
              mid = pmid - &primitiveInfo[0];
            } else {
              // Create leaf _BVHBuildNode_
              int firstPrimOffset = orderedPrims.size();
              for (int i = start; i < end; ++i) {
                int primNum = primitiveInfo[i].primitiveNumber;
                orderedPrims.push_back(primitives[primNum]);
              }
              node->InitLeaf(firstPrimOffset, nPrimitives, bounds);
              return node;
            }
          }
          break;
        }
      }
      node->InitInterior(
          dim,
          recursiveBuild(primitiveInfo, start, mid, totalNodes, orderedPrims),
          recursiveBuild(primitiveInfo, mid, end, totalNodes, orderedPrims));
    }
  }
  return node;
}

bool BVHAccel::Intersect(const Ray& ray, SurfaceInteraction* isect) {
  bool hit = false;
  glm::vec3 invDir(1.0f / ray.direction.x, 1.0f / ray.direction.y,
                   1.0f / ray.direction.z);
  int dirIsNeg[3] = {invDir.x < 0, invDir.y < 0, invDir.z < 0};

  int toVisitOffset = 0;
  BVHBuildNode* node = nodes[0];
  BVHBuildNode* stack[64];

  while (true) {
    if (node->bounds.Intersect(ray)) {
      if (node->nPrimitives > 0) {
        // Intersect ray with primitives in leaf BVH node
        for (int i = 0; i < node->nPrimitives; ++i)
          if (primitives[node->firstPrimOffset + i]->Intersect(ray, isect))
            hit = true;
        if (toVisitOffset == 0) break;
        node = stack[--toVisitOffset];
      } else {
        // Put far BVH node on nodesToVisit stack, advance to near node
        if (dirIsNeg[node->splitAxis]) {
          stack[toVisitOffset++] = node->children[0];
          node = node->children[1];
        } else {
          stack[toVisitOffset++] = node->children[1];
          node = node->children[0];
        }
      }
    } else {
      if (toVisitOffset == 0) break;
      node = stack[--toVisitOffset];
    }
  }

  return hit;
}

std::vector<Primitive*> BVHAccel::getAllLights() {
  std::vector<Primitive*> lights;
  for (auto& p : primitives) {
    if (p->Emissive()) {
      lights.push_back(p.get());
    }
  }
  return lights;
}
