#include "triangle.h"

#include "../global.h"
#include "../scene.h"
#include "aabb.h"
#include "ray.h"

Triangle::Triangle(std::array<const Vertex *, 3> &_v, const Material *_mat)
    : v(_v), material(_mat) {
  glm::vec3 a = v[1]->position - v[0]->position;
  glm::vec3 b = v[2]->position - v[0]->position;
  normal = glm::cross(a, b);
  normal = glm::normalize(normal);
}

glm::vec3 Triangle::Center() const {
  return (v[0]->position + v[1]->position + v[2]->position) / 3.0f;
}

glm::vec3 Triangle::GetNormal() const { return normal; }

size_t Triangle::GetID() const { return id; }

void Triangle::SetID(size_t id) { this->id = id; }

const Material *Triangle::GetMaterial() const { return material; }

float Triangle::Area() const {
  glm::vec3 a = v[1]->position - v[0]->position;
  glm::vec3 b = v[2]->position - v[0]->position;
  glm::vec3 c = glm::cross(a, b);
  return sqrtf(glm::dot(c, c)) / 2;
}

glm::vec3 Triangle::RandomPoint() const {
  glm::vec3 a = v[1]->position - v[0]->position;
  glm::vec3 b = v[2]->position - v[0]->position;
  int temp = rand() % 1000;
  float p = (float)temp / 1000.0f;
  float q = (float)temp / 1000.0f;
  return v[0]->position * (1 - sqrtf(p)) + v[1]->position * sqrtf(p) * (1 - q) +
         v[2]->position * sqrtf(p) * q;
}

AABB Triangle::WorldBound() {
  float xmin = FLT_MAX;
  float ymin = FLT_MAX;
  float zmin = FLT_MAX;
  float xmax = FLT_MIN;
  float ymax = FLT_MIN;
  float zmax = FLT_MIN;

  for (auto e : v) {
    xmin = xmin < e->position.x ? xmin : e->position.x;
    ymin = ymin < e->position.y ? ymin : e->position.y;
    zmin = zmin < e->position.z ? zmin : e->position.z;
    xmax = xmax > e->position.x ? xmax : e->position.x;
    ymax = ymax > e->position.y ? ymax : e->position.y;
    zmax = zmax > e->position.z ? zmax : e->position.z;
  }

  AABB bound;
  bound.pMin = glm::vec3(xmin, ymin, zmin);
  bound.pMax = glm::vec3(xmax, ymax, zmax);
  return bound;
}

bool Triangle::Intersect(const Ray &ray, SurfaceInteraction *isect) {
  glm::vec3 a = v[0]->position - v[2]->position;
  glm::vec3 b = v[1]->position - v[2]->position;
  glm::vec3 c = ray.origin - v[2]->position;

  const glm::vec3 direction = ray.direction;

  float D = direction.x * a.y * b.z + b.x * direction.y * a.z +
            a.x * b.y * direction.z - a.x * direction.y * b.z -
            b.x * a.y * direction.z - direction.x * b.y * a.z;

  if (D < epsilon && D > -epsilon) return false;
  float alpha = (direction.x * c.y * b.z + b.x * direction.y * c.z +
                 c.x * b.y * direction.z - c.x * direction.y * b.z -
                 b.x * c.y * direction.z - direction.x * b.y * c.z) /
                D;
  float beta = (direction.x * a.y * c.z + c.x * direction.y * a.z +
                a.x * c.y * direction.z - a.x * direction.y * c.z -
                c.x * a.y * direction.z - direction.x * c.y * a.z) /
               D;
  float t = -(c.x * a.y * b.z + b.x * c.y * a.z + a.x * b.y * c.z -
              a.x * c.y * b.z - b.x * a.y * c.z - c.x * b.y * a.z) /
            D;
  if (t < epsilon || alpha < 0 || beta < 0 || alpha + beta > epsilon + 1)
    return false;

  auto barycentricCoord = glm::vec3(alpha, beta, 1 - alpha - beta);

  if (t < isect->time) {
    isect->normal = alpha * v[0]->normal + beta * v[1]->normal +
                    (1 - alpha - beta) * v[2]->normal;
    isect->normal = GetNormal();
    isect->material = this->material;
    isect->hitID = this->id;
    isect->time = t;
  }

  return true;
}

bool Triangle::Emissive() {
  if (material->emission.x > 0 || material->emission.y > 0 ||
      material->emission.z > 0) {
    return true;
  }
  return false;
}
