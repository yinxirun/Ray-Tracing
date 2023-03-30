#ifndef _PATHTRACING_H
#define _PATHTRACING_H
#include <cmath>
#include <cstdlib>

#include "BVH.h"
#include "Camera.h"
#include "Material.h"
#include "Mesh.h"
#include "Ray.h"
#include "Scene.h"
#include "Vec.h"
using std::cout;
using std::endl;

class PathTracing {
  float p;       ///< Probability of being discarded.
  unsigned spp;  ///< Samples per pixel.
  Scene *scene;
  Camera *camera;
  BVH *bvh;

  /// @brief Phong illumination model. Please assure that input vectors are
  /// normalized.
  /// @param inc_dir Direction of incident light.
  /// @param ref_dir Direction of reflected light.
  /// @return Value of BRDF.
  Vector3D BRDF(Vector3D inc, Vector3D refl, Material *mtl, Vector3D n);

  /// @brief Generate ray originally. (0, 0) means the left-bottom pixel of the
  /// image. X coordinate increases rightwards. Y coordinate increases upwards.
  /// @param x The x coordinate of pixel the ray passed through.
  /// @param y The y coordinate of pixel the ray passed through.
  /// @return A ray.
  Ray emit(unsigned x, unsigned y);

  /// @brief Trace a ray recursively.
  Vector3D trace(Ray ray);

  Vector3D shade(Vector3D point, Vector3D normal, Material *m, Vector3D wo,
                 unsigned id);

 public:
  void render();

  PathTracing(Scene *s, Camera *c, unsigned _spp, float _p);

  bool enable_BVH();
};

#endif