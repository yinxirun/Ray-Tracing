#ifndef _BVH_H
#define _BVH_H
#include <string>
#include <vector>

#include "Mesh.h"
#include "Ray.h"
#include "Vec.h"

/// Node of bounding volume tree.
struct Node {
  Vector3D diagonal_vertices[2];  ///< bounding box's two diagonal vertices,
                                  ///< representing a axis-aligned bounding box.
  std::vector<unsigned int> face_indices;
  struct Node *left;
  struct Node *right;
};

/// AABB(Axis-aligned Box Bounding) Bounding Volume Hierarchy
class BVH {
 public:
  struct Node *rootNode;
  Mesh *mesh;

  void divide_SAH(struct Node *root);

  void divide(struct Node *root, int axis);

  BVH();
  ~BVH();

  /// @brief construct from .obj file.
  /// @param filepath path of the .obj file
  /// @return true or false
  bool construct_from_obj(const std::string filepath);

  bool construct_from_mesh(Mesh *mesh);

  bool intersect(Ray ray, Node *node, float &t, unsigned &faceid);

  /// @brief delete all data in this tree
  /// @details use postorder traversal to delete data in binary tree recursively
  void destroy();

  void seek(std::vector<unsigned> &faces, Node *left, Node *right, float &cost);
};
#endif