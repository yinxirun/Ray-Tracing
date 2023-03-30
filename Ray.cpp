#include "Ray.h"
Ray::Ray() {}

Ray::Ray(const Vector3D &o,const Vector3D &d) : origin(o), direction(d) {}

Ray::~Ray() {}

void Ray::assign(Vector3D& o,Vector3D d) {
  origin = o;
  direction = d;
}