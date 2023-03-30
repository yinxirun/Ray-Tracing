#include "BVH.h"

#include <algorithm>
#include <iostream>
#include <stack>
#include <vector>

#include "Mesh.h"
#include "Vec.h"

extern const float epsilon;

float max(float a, float b) { return a < b ? b : a; }

float min(float a, float b) { return a < b ? a : b; }

void swap(float &a, float &b) {
  float temp = a;
  a = b;
  b = temp;
}

BVH::BVH() { rootNode = nullptr; }

BVH::~BVH() { destroy(); }

bool BVH::construct_from_obj(const std::string filepath) { return true; }

void BVH::divide(struct Node *root, int axis) {
  float xmin = __FLT_MAX__;
  float ymin = __FLT_MAX__;
  float zmin = __FLT_MAX__;
  float xmax = __FLT_MIN__;
  float ymax = __FLT_MIN__;
  float zmax = __FLT_MIN__;
  for (std::vector<unsigned>::iterator i = root->face_indices.begin();
       i != root->face_indices.end(); ++i) {
    for (int j = 0; j < 3; j++) {
      unsigned vertex_id = mesh->get_faces()[*i].vertices[j];
      Vector3D point = mesh->get_attr().vertices[vertex_id];
      if (point.x > xmax) xmax = point.x;
      if (point.y > ymax) ymax = point.y;
      if (point.z > zmax) zmax = point.z;
      if (point.x < xmin) xmin = point.x;
      if (point.y < ymin) ymin = point.y;
      if (point.z < zmin) zmin = point.z;
    }
  }
  root->diagonal_vertices[0].x = xmin;
  root->diagonal_vertices[0].y = ymin;
  root->diagonal_vertices[0].z = zmin;
  root->diagonal_vertices[1].x = xmax;
  root->diagonal_vertices[1].y = ymax;
  root->diagonal_vertices[1].z = zmax;

  std::sort(root->face_indices.begin(), root->face_indices.end(),
            [&](unsigned a, unsigned b) -> bool {
              Vector3D a_centre = mesh->get_faces()[a].center;
              Vector3D b_centre = mesh->get_faces()[b].center;
              float a_;
              float b_;
              switch (axis) {
                case 0:
                  a_ = a_centre.x;
                  b_ = b_centre.x;
                  break;
                case 1:
                  a_ = a_centre.y;
                  b_ = b_centre.y;
                  break;
                case 2:
                  a_ = a_centre.z;
                  b_ = b_centre.z;
                  break;
                default:
                  break;
              }
              if (a_ < b_)
                return true;
              else
                return false;
            });

  if (root->face_indices.size() == 1) {
    root->left = nullptr;
    root->right = nullptr;
    return;
  } else {
    root->left = new Node;
    root->right = new Node;

    unsigned left_size = root->face_indices.size() / 2;
    unsigned right_size = root->face_indices.size() - left_size;
    root->left->face_indices.assign(root->face_indices.begin(),
                                    root->face_indices.begin() + left_size);
    root->right->face_indices.assign(root->face_indices.begin() + left_size,
                                     root->face_indices.end());
    divide(root->left, (++axis) % 3);
    divide(root->right, (++axis) % 3);
  }
}

bool BVH::construct_from_mesh(Mesh *m) {
  mesh = m;
  int axis = 0;
  rootNode = new struct Node;
  rootNode->left = nullptr;
  rootNode->right = nullptr;
  for (unsigned i = 0; i < m->num_faces(); i++) {
    rootNode->face_indices.emplace_back(i);
  }
  //divide_SAH(rootNode);
  divide(rootNode, 1);
  return true;
}

void BVH::destroy() {
  if (rootNode == nullptr) return;

  // "output" is a stack containing elements pushed in the order of
  // middle-right-left. In the end, the order in which elements pop is
  // left-right-middle, that is the output of postorder traversal.
  std::stack<struct Node *> output;
  std::stack<struct Node *> process;
  process.push(rootNode);
  struct Node *p;
  while (!process.empty()) {
    p = process.top();
    process.pop();
    output.push(p);
    if (p->left != nullptr) process.push(p->left);
    if (p->right != nullptr) process.push(p->right);
  }
  while (!output.empty()) {
    p = output.top();
    output.pop();
    delete p;
  }
}

bool BVH::intersect(Ray ray, Node *node, float &t, unsigned &faceid) {
  if (node == rootNode) {
    faceid = UINT32_MAX;
    t = __FLT_MAX__;
  }

  // 判断光线是否与包围盒相交。如果不相交，立即返回false。
  // BEGIN---------------------------------------------------------------------
  float x_in = __FLT_MIN__;
  float x_out = __FLT_MAX__;
  if (ray.direction.x < epsilon && ray.direction.x > -epsilon) {
    if (ray.origin.x > node->diagonal_vertices[1].x) return false;
    if (ray.origin.x < node->diagonal_vertices[0].x) return false;
  } else {
    x_in = (node->diagonal_vertices[0].x - ray.origin.x) / ray.direction.x;
    x_out = (node->diagonal_vertices[1].x - ray.origin.x) / ray.direction.x;
    if (x_in > x_out) swap(x_in, x_out);
  }

  float y_in = __FLT_MIN__;
  float y_out = __FLT_MAX__;
  if (ray.direction.y < epsilon && ray.direction.y > -epsilon) {
    if (ray.origin.y > node->diagonal_vertices[1].y) return false;
    if (ray.origin.y < node->diagonal_vertices[0].y) return false;
  } else {
    y_in = (node->diagonal_vertices[0].y - ray.origin.y) / ray.direction.y;
    y_out = (node->diagonal_vertices[1].y - ray.origin.y) / ray.direction.y;
    if (y_in > y_out) swap(y_in, y_out);
  }

  float z_in = __FLT_MIN__;
  float z_out = __FLT_MAX__;
  if (ray.direction.z < epsilon && ray.direction.z > -epsilon) {
    if (ray.origin.z > node->diagonal_vertices[1].z) return false;
    if (ray.origin.z < node->diagonal_vertices[0].z) return false;
  } else {
    z_in = (node->diagonal_vertices[0].z - ray.origin.z) / ray.direction.z;
    z_out = (node->diagonal_vertices[1].z - ray.origin.z) / ray.direction.z;
    if (z_in > z_out) swap(z_in, z_out);
  }

  float t_in = max(x_in, max(y_in, z_in));
  float t_out = min(x_out, min(y_out, z_out));
  if (t_in > t_out || t_out <= 0) return false;
  // END--------------------------------------------------------------------------------

  if (node->left == nullptr && node->right == nullptr) {
    float temp = mesh->get_faces()[node->face_indices[0]].intersect(ray);
    if (temp == __FLT_MIN__) return false;
    if (temp < t) {
      t = temp;
      faceid = node->face_indices[0];
    }
    return true;
  } else if (node->left != nullptr && node->right != nullptr) {
    bool left_outcome = intersect(ray, node->left, t, faceid);
    bool right_outcome = intersect(ray, node->right, t, faceid);
    if (left_outcome | right_outcome)
      return true;
    else
      return false;
  } else {
    std::cout << "What the fuck!" << std::endl;
    return false;
  }
}

void BVH::divide_SAH(struct Node *root) {
  float cost = __FLT_MAX__;
  unsigned left_num;
  char best_axis;

  float xmin = __FLT_MAX__;
  float ymin = __FLT_MAX__;
  float zmin = __FLT_MAX__;
  float xmax = __FLT_MIN__;
  float ymax = __FLT_MIN__;
  float zmax = __FLT_MIN__;
  for (int i = 0; i < root->face_indices.size(); ++i) {
    Triangle temp = mesh->get_faces()[root->face_indices[i]];
    xmin = min(temp.diagonal_vertices[0].x, xmin);
    ymin = min(temp.diagonal_vertices[0].y, ymin);
    zmin = min(temp.diagonal_vertices[0].z, zmin);
    xmax = max(temp.diagonal_vertices[1].x, xmax);
    ymax = max(temp.diagonal_vertices[1].y, ymax);
    zmax = max(temp.diagonal_vertices[1].z, zmax);
  }
  root->diagonal_vertices[0].assign(xmin, ymin, zmin);
  root->diagonal_vertices[1].assign(xmax, ymax, zmax);

  if (root->face_indices.size() == 1) {
    root->left = nullptr;
    root->right = nullptr;
    return;
  }

  root->left = new Node();
  root->right = new Node();

  std::sort(root->face_indices.begin(), root->face_indices.end(),
            [&](unsigned a, unsigned b) -> bool {
              Vector3D a_centre = mesh->get_faces()[a].center;
              Vector3D b_centre = mesh->get_faces()[b].center;
              return a_centre.x < b_centre.x;
            });
  seek(root->face_indices, root->left, root->right, cost);
  std::sort(root->face_indices.begin(), root->face_indices.end(),
            [&](unsigned a, unsigned b) -> bool {
              Vector3D a_centre = mesh->get_faces()[a].center;
              Vector3D b_centre = mesh->get_faces()[b].center;
              return a_centre.y < b_centre.y;
            });
  seek(root->face_indices, root->left, root->right, cost);
  std::sort(root->face_indices.begin(), root->face_indices.end(),
            [&](unsigned a, unsigned b) -> bool {
              Vector3D a_centre = mesh->get_faces()[a].center;
              Vector3D b_centre = mesh->get_faces()[b].center;
              return a_centre.z < b_centre.z;
            });
  seek(root->face_indices, root->left, root->right, cost);

  root->left->face_indices.assign(root->face_indices.begin(),
                                  root->face_indices.begin() + left_num);
  root->right->face_indices.assign(root->face_indices.begin() + left_num,
                                   root->face_indices.end());
  if (root->left->face_indices.size() < 8)
    divide(root->left, 0);
  else
    divide_SAH(root->left);
  if (root->left->face_indices.size() < 8)
    divide(root->right, 0);
  else
    divide_SAH(root->right);

  if (root->face_indices.size() != 1)
    std::vector<unsigned>().swap(root->face_indices);
}

void BVH::seek(std::vector<unsigned> &face_ids, Node *left, Node *right,
               float &cost) {
  float side0, side1, side2;
  unsigned total = face_ids.size();
  unsigned left_num;
  std::vector<Triangle> &faces = mesh->get_faces();

  std::vector<float> surface_left;
  std::vector<float> surface_right;
  surface_left.resize(total - 1);
  surface_right.resize(total - 1);

  float xmin = __FLT_MAX__;
  float ymin = __FLT_MAX__;
  float zmin = __FLT_MAX__;
  float xmax = __FLT_MIN__;
  float ymax = __FLT_MIN__;
  float zmax = __FLT_MIN__;
  for (int i = 0; i < total - 1; ++i) {
    Triangle temp = faces[face_ids[i]];
    xmin = min(temp.diagonal_vertices[0].x, xmin);
    ymin = min(temp.diagonal_vertices[0].y, ymin);
    zmin = min(temp.diagonal_vertices[0].z, zmin);
    xmax = max(temp.diagonal_vertices[1].x, xmax);
    ymax = max(temp.diagonal_vertices[1].y, ymax);
    zmax = max(temp.diagonal_vertices[1].z, zmax);
    side0 = xmax - xmin;
    side1 = ymax - ymin;
    side2 = zmax - zmin;
    surface_left[i] = side0 * side1 + side1 * side2 + side2 * side0;
  }
  xmin = __FLT_MAX__;
  ymin = __FLT_MAX__;
  zmin = __FLT_MAX__;
  xmax = __FLT_MIN__;
  ymax = __FLT_MIN__;
  zmax = __FLT_MIN__;
  for (int i = total - 1; i >= 1; --i) {
    Triangle temp = faces[face_ids[i]];
    xmin = min(temp.diagonal_vertices[0].x, xmin);
    ymin = min(temp.diagonal_vertices[0].y, ymin);
    zmin = min(temp.diagonal_vertices[0].z, zmin);
    xmax = max(temp.diagonal_vertices[1].x, xmax);
    ymax = max(temp.diagonal_vertices[1].y, ymax);
    zmax = max(temp.diagonal_vertices[1].z, zmax);
    side0 = xmax - xmin;
    side1 = ymax - ymin;
    side2 = zmax - zmin;
    surface_right[i - 1] = side0 * side1 + side1 * side2 + side2 * side0;
  }

  bool flag = false;
  for (int i = 0; i < total - 1; ++i) {
    float temp_cost =
        surface_left[i] * (i + 1) + surface_right[i] * (total - 1 - i);
    if (temp_cost < cost) {
      cost = temp_cost;
      left_num = i + 1;
      flag = true;
    }
  }
  if (flag) {
    left->face_indices.assign(face_ids.begin(), face_ids.begin() + left_num);
    right->face_indices.assign(face_ids.begin() + left_num, face_ids.end());
    }
}