#ifndef BVH_H
#define BVH_H

#include "scene.h"

class bvhNode {
 public:
  bvhNode* left = nullptr;
  bvhNode* right = nullptr;
  std::vector<unsigned> faceIndices;
  std::array<glm::vec3, 2> box;
};

class BVH {
 public:
  BVH(Scene& scene);
  ~BVH();
  inline unsigned getNodeNum() const { return nodeNum; }
  inline unsigned getLeafNodeNum() const { return leafNodeNum; }
  inline const bvhNode* getRootNode() const { return root; }
  const Triangle* getFace(unsigned index);
  std::vector<const Triangle*>& getAllLights() { return lights; }

 private:
  bvhNode* root;
  unsigned nodeNum;
  unsigned leafNodeNum;
  std::vector<Triangle> faces;
  std::vector<const Triangle*> lights;
  bvhNode* build(std::vector<unsigned>& indices);
  void clear(bvhNode* node);
};

#endif