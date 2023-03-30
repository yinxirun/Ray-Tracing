#ifndef _MATERIAL_H
#define _MATERIAL_H
#include <string>
#include <unordered_set>
#include <vector>

#include "Vec.h"
class Material {
 public:
  unsigned id;
  std::string name;
  Vector3D ambient;                // Ka
  Vector3D diffuse;                // Kd  diffuse coefficient
  Vector3D specular;               // Ks  specular coefficient
  Vector3D emission;               // Ke
  Vector3D transformation_filter;  // Tf  maybe correspond to Phong exponent??
  int illum;
  float dissolve;         // d
  Vector3D transparency;  // Tr
  float shineness;        // Ns
  float refraction;       // Ni
  unsigned char* map;
  int map_width, map_height, map_nrChannels;
  void bind_map(std::string);
  Material();
  ~Material();
};

class AreaLight {
 public:
  Vector3D radiance;
  Material* mtl;
  std::vector<unsigned> faces_id;
};
#endif