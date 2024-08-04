#pragma once

#include "glm/glm.hpp"

class Ray {
 public:
  Ray(glm::vec3 o, glm::vec3 d) : origin(o), direction(d) {};
  glm::vec3 origin;
  glm::vec3 direction;

  float tMax = FLT_MAX;
};