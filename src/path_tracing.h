#pragma once

#include <random>

#include "camera.h"
#include "scene.h"

class PathTracingSolver {
 public:
  PathTracingSolver(unsigned spp = 1024, float rr = 0.5);
  void SetCamera(Camera *c);
  void RenderAndSave(Scene &scene, BVHAccel &bvh, bool denoise = false);
  void SaveColorImageLDR(std::string fileName);
  void SaveNormalImage(std::string fileName);
  void SavePositionImage(std::string fileName);
  void SaveTriangleIdImage(std::string fileName);

  void SaveColorImageHDR(std::string fileName,std::vector<glm::vec3>& hdrImage);

 private:
  std::vector<glm::vec3> colorImage;
  std::vector<glm::vec3> normalImage;
  std::vector<glm::vec3> positionImage;
  std::vector<glm::uint32> triangleIDImage;
  std::vector<glm::vec3> colorImageDenoised;
  std::vector<glm::vec3> ldrImage;

  Camera *camera = nullptr;

  std::vector<Primitive*> lights;

  float rr;
  unsigned sampleNum;
  std::default_random_engine e;
  // 0  1
  std::uniform_real_distribution<float> u;

  Ray emitRay(unsigned x, unsigned y);

  glm::vec3 shade(BVHAccel &bvh, glm::vec3 hitPoint, glm::vec3 normal,
                  const Material *material, glm::vec3 out);

  glm::vec3 BRDF(glm::vec3 in, glm::vec3 out, const Material *material);

  void Sample(const Material *material, glm::vec3 normal, glm::vec3 &d,
              float &pdf);

  void ToneMapping(std::vector<glm::vec3>& hdrImage);

  void Denoise();
};