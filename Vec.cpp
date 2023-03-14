#include "Vec.h"

Vector3D operator+(const Vector3D& a, const Vector3D& b) {
  return Vector3D(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vector3D operator-(const Vector3D& a, const Vector3D& b) {
  return Vector3D(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vector3D operator*(const Vector3D& a, const Vector3D& b) {
  return Vector3D(a.x * b.x, a.y * b.y, a.z * b.z);
}

Vector3D operator*(float a, const Vector3D& b) {
  return Vector3D(a * b.x, a * b.y, a * b.z);
}

Vector3D operator/(const Vector3D& a, float b) {
  Vector3D ret(0, 0, 0);
  ret.x = a.x / b;
  ret.y = a.y / b;
  ret.z = a.z / b;
  return ret;
}

Vector3D Vector3D::random() {
  float x = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) *
            pow(-1, rand() % 2);
  float y = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) *
            pow(-1, rand() % 2);
  float z = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) *
            pow(-1, rand() % 2);
  return Vector3D(x, y, z);
}

Vector3D Vector3D::cross(const Vector3D& a, const Vector3D& b) {
  return Vector3D(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                  a.x * b.y - a.y * b.x);
}
float Vector3D::dot(const Vector3D& a, const Vector3D& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

void Vector3D::normalize() {
  float len = length();
  if (len == 0) return;
  x /= len;
  y /= len;
  z /= len;
}

Vector3D::Vector3D(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

Vector3D::Vector3D() : x(0), y(0), z(0) {}

float Vector3D::length() { return sqrt(x * x + y * y + z * z); }

Vector3D Vector3D::operator-() { return Vector3D(-x, -y, -z); }

void Vector3D::assign(float _x, float _y, float _z) {
  x = _x;
  y = _y;
  z = _z;
}