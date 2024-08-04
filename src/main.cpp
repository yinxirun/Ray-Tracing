#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

#include "bvh_accel.h"
#include "nlohmann/json.hpp"
#include "path_tracing.h"
#include "scene.h"

const float pi = 3.1415926535;

const float epsilon = 0.001;

int main() {

  nlohmann::json configLoader;
  std::fstream f("config.json", std::ios::in);
  f >> configLoader;

  unsigned spp = configLoader.at("spp");
  float rr = configLoader.at("rr");
  bool denoise = configLoader.at("denoise");
  int sceneTorender = configLoader.at("scene_to_render");
  auto sceneConfig = configLoader.at("scenes")[sceneTorender];
  std::string scenePath = sceneConfig.at("path");
  auto cameraConfig = sceneConfig.at("camera");

  auto temp = cameraConfig.at("position");
  glm::vec3 position = glm::vec3(temp[0], temp[1], temp[2]);
  temp = cameraConfig.at("lookat");
  glm::vec3 lookat = glm::vec3(temp[0], temp[1], temp[2]);
  temp = cameraConfig.at("up");
  glm::vec3 up = glm::vec3(temp[0], temp[1], temp[2]);
  float fovy = cameraConfig.at("fovy");
  unsigned width = cameraConfig.at("width");
  unsigned height = cameraConfig.at("height");
  float near = cameraConfig.at("near");
  float far = cameraConfig.at("far");

  Camera camera;
  camera.SetParam(position, lookat, up);
  camera.SetParam(fovy / 180 * pi, near, far, width, height);

  clock_t start = clock();
  Scene scene;
  scene.load(scenePath);
  clock_t end = clock();
  std::cout << "Time to load models is " << end - start << "ms." << std::endl;

  start = clock();
  auto primitives = scene.AllTriangles();
  BVHAccel accelerator(primitives, 100000000, SplitMethod::SAH);
  end = clock();
  std::cout << "Time to build BVH is " << end - start << "ms." << std::endl;

  PathTracingSolver pt(spp, rr);
  pt.SetCamera(&camera);
  pt.RenderAndSave(scene, accelerator, denoise);

  std::cout << "Over" << std::endl;
}