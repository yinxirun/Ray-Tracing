#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

#include "bvh.h"
#include "path_tracing.h"

// const std::string modelPath = "./models/veach-mis/veach-mis.obj";
std::string modelPath = "./models/cornell-box/cornell-box.obj";

const unsigned width = 512;

const unsigned height = 512;

std::fstream recorder;

int main() {
  recorder.open("./log.txt", std::ios::out);
  clock_t start = clock();
  Scene scene;
  scene.load(modelPath);
  clock_t end = clock();
  std::cout << "Time to load models is " << end - start << "ms." << std::endl;

  start = clock();
  BVH bvh(scene);
  end = clock();
  std::cout << "Time to build BVH is " << end - start << "ms." << std::endl;
  std::cout << "Number of BVH's nodes is " << bvh.getNodeNum() << "."
            << std::endl;
  std::cout << "Number of BVH's leaf nodes is " << bvh.getLeafNodeNum() << "."
            << std::endl;

  Camera camera;
  PathTracingSolver pt(width, height);
  pt.draw(scene, bvh, camera);
  std::cout << "over" << std::endl;
  recorder.close();
}