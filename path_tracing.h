#ifndef PATH_TRACING_H
#define PATH_TRACING_H
#include <random>

#include "bvh.h"

struct IntersectionInfo {
  bool intersection;
  float time;
  const Triangle* face;
  IntersectionInfo();
};

class Ray {
 public:
  Ray(glm::vec3 o, glm::vec3 d);
  IntersectionInfo intersect(const Triangle* face);
  void intersect(BVH& bvh, const bvhNode* node, IntersectionInfo& info);
  glm::vec3 origin;
  glm::vec3 direction;
};

struct Camera {
  Camera();
  glm::vec3 position;
  glm::vec3 up;
  glm::vec3 forward;
  float fovy;
};

class PathTracingSolver {
 public:
  PathTracingSolver(unsigned w, unsigned h);
  void draw(Scene& scene, BVH& bvh, Camera& camera);
  void save(std::string fileName);

 private:
  unsigned width;
  unsigned height;
  std::vector<glm::vec3> frame;
  float rr = 0.5;
  unsigned sampleNum;
  std::uniform_real_distribution<float> u0;
  std::uniform_real_distribution<float> u1;
  std::default_random_engine e;

  Ray emitRay(unsigned x, unsigned y, Camera& camera);
  glm::vec3 shade(Scene& scene, BVH& bvh, glm::vec3 hitPoint,
                  const Triangle* face, glm::vec3 out);
  glm::vec3 BRDF(glm::vec3 in, glm::vec3 out, const Material* material);
  void toneMapping();
};

#endif