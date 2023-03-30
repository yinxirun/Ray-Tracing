#ifndef _RAY_H
#define _RAY_H
#include "Vec.h"
/// Ray is expressed by parametric equation: origin + t direction, where t is
/// the parameter.
class Ray {
 public:
  Vector3D origin;
  Vector3D direction;
  Ray();
  Ray(const Vector3D &origin,const Vector3D &direction);
  ~Ray();
  void assign(Vector3D& o,Vector3D d);
};

#endif