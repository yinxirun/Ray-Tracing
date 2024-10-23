#include "camera.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/transform.hpp"

void Camera::SetParam(glm::vec3 eye, glm::vec3 lookat, glm::vec3 up)
{
  this->position = eye;
  this->forward = glm::normalize(lookat - eye);
  glm::vec3 right = glm::cross(this->forward, up);
  this->up = glm::normalize(glm::cross(right, forward));
}

void Camera::SetParam(float fovy, float near, float far, unsigned width, unsigned height)
{
  this->fovy = fovy;
  this->near = near;
  this->far = far;
  this->width = width;
  this->height = height;
}

glm::mat4 Camera::View()
{
  return glm::lookAt(position, position + forward, up);
}

glm::mat4 Camera::Perspective()
{
  float aspect = static_cast<float>(width) / static_cast<float>(height);
  return glm::perspective(fovy, aspect, near, far);
}
