#include "bvh.h"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>

#include "float.h"
int axis = 0;
extern const float epsilon;

BVH::BVH(Scene& scene) {
  nodeNum = 0;
  leafNodeNum = 0;
  faces = scene.getAllFaces();

  for (int i = 0; i < faces.size(); ++i) {
    glm::vec3 e = faces[i].getMaterial()->emissive;
    if (e[0] > epsilon || e[1] > epsilon || e[2] > epsilon) {
      lights.push_back(&faces[i]);
    }
  }

  std::vector<unsigned> indices(faces.size());
  std::iota(indices.begin(), indices.end(), 0);
  root = build(indices);
}

BVH::~BVH() { clear(root); }

bvhNode* BVH::build(std::vector<unsigned>& indices) {
  bvhNode* node = new bvhNode;
  nodeNum += 1;
  float xmin = FLT_MAX;
  float ymin = FLT_MAX;
  float zmin = FLT_MAX;
  float xmax = FLT_MIN;
  float ymax = FLT_MIN;
  float zmax = FLT_MIN;
  for (int i = 0; i < indices.size(); ++i) {
    for (int j = 0; j < 3; ++j) {
      auto pos = faces[indices[i]].getPosition(j);
      xmin = std::min<float>(xmin, pos[0]);
      ymin = std::min<float>(ymin, pos[1]);
      zmin = std::min<float>(zmin, pos[2]);
      xmax = std::max<float>(xmax, pos[0]);
      ymax = std::max<float>(ymax, pos[1]);
      zmax = std::max<float>(zmax, pos[2]);
    }
  }
  node->box[0] = glm::vec3(xmin, ymin, zmin);
  node->box[1] = glm::vec3(xmax, ymax, zmax);

  if (indices.size() <= 1) {
    leafNodeNum += 1;
    node->faceIndices = indices;
    return node;
  }

  int axis = rand() % 3;

  std::sort(indices.begin(), indices.end(),
            [=](unsigned a, unsigned b) -> bool {
              glm::vec3 ca = faces[a].center();
              glm::vec3 cb = faces[b].center();
              return ca[axis] < cb[axis];
            });
  std::vector<unsigned> leftFaces;
  std::vector<unsigned> rightFaces;
  size_t halfSize = indices.size() / 2;
  leftFaces.assign(indices.begin(), indices.begin() + halfSize);
  rightFaces.assign(indices.begin() + halfSize, indices.end());

  node->left = build(leftFaces);
  node->right = build(rightFaces);
  return node;
}

void BVH::clear(bvhNode* node) {
  if (node == nullptr) return;
  clear(node->left);
  clear(node->right);
  delete node;
  node = nullptr;
}

const Triangle* BVH::getFace(unsigned index) {
  if (index < 0 || index >= faces.size()) return nullptr;
  return &faces[index];
}