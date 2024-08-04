#pragma once
#include "glm/glm.hpp"
#include "global.h"

struct Camera {
  void SetParam(glm::vec3 eye, glm::vec3 lookat, glm::vec3 up);
  void SetParam(float fovy, float near, float far, unsigned width,
                unsigned height);
  glm::mat4 View();
  glm::mat4 Perspective();

  glm::vec3 position;
  glm::vec3 up;
  glm::vec3 forward;

  // 弧度
  float fovy;
  float near = 0.1;
  float far = 10000;
  unsigned width;
  unsigned height;
};