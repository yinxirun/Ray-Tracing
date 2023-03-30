#ifndef _MESH_H
#define _MESH_H

#include <string>
#include <vector>

#include "Material.h"
#include "Ray.h"
#include "Vec.h"

class Attribute {
 public:
  std::vector<Vector3D> vertices;
  std::vector<Vector3D> normals;
  std::vector<Vector3D> texcoords;

  void add_vertex(Vector3D);
  void add_normal(Vector3D);
  void add_texcoord(Vector3D);
};

class Triangle {
 private:
  Attribute* attr;

 public:
  std::vector<unsigned> vertices;
  std::vector<unsigned> normals;
  std::vector<unsigned> texcords;
  Material* material;
  Vector3D diagonal_vertices[2];
  Vector3D center;
  Vector3D face_normal;

  Triangle();
  ~Triangle();
  void set_attr(Attribute* a);
  Vector3D centre();
  float intersect(Ray r);
  Vector3D normal_average();
  float area();
  void init();
  Vector3D texture_mapping(Vector3D point);
};

class Mesh {
 public:
  Attribute attr;
  std::vector<Triangle> faces;

  unsigned num_faces();
  unsigned num_vertices();
  unsigned num_normals();
  unsigned num_texcoords();
  Attribute& get_attr();
  std::vector<Triangle>& get_faces();
  void add_face(Triangle&);
};

#endif