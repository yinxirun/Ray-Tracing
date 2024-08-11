#pragma once

#include <vector>

#include "glm/glm.hpp"

class FXAA {
 public:
  std::vector<glm::vec3> operator()(std::vector<glm::vec3>& input,
                                    unsigned width, unsigned height);

 private:
  std::vector<glm::vec3> pixels;
  int width;
  int height;

  glm::vec3 texture(std::vector<glm::vec3>&, glm::ivec2);

  glm::vec3 texture(std::vector<glm::vec3>&, glm::vec2);
};