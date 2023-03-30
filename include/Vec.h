#ifndef _VECTOR3D_H
#define _VECTOR3D_H

#include <cmath>

class Vector3D {
 public:
  float x, y, z;
  Vector3D();
  Vector3D(float _x, float _y, float _z);

  float length();

  void normalize();

  void assign(float _x,float _y,float _z);

  Vector3D operator-();
  friend Vector3D operator+(const Vector3D& a, const Vector3D& b);
  friend Vector3D operator-(const Vector3D& a, const Vector3D& b);
  friend Vector3D operator*(const Vector3D& a, const Vector3D& b);
  friend Vector3D operator*(float a, const Vector3D& b);
  friend Vector3D operator/(const Vector3D& a, float b);

  static Vector3D cross(const Vector3D& a, const Vector3D& b);
  static float dot(const Vector3D& a, const Vector3D& b);

  /// Return a random vector of which every element is between 0 and 1.
  static Vector3D random();
};

#endif