#pragma once

class AABB;
class Ray;
class SurfaceInteraction;

class Primitive {
 public:
  virtual AABB WorldBound() = 0;
  virtual bool Intersect(const Ray &, SurfaceInteraction *) = 0;
  virtual bool Emissive()=0;
};