#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

#include "Camera.h"
#include "Importer.h"
#include "PathTracing.h"
#include "Scene.h"
#include "Vec.h"
using std::cout;
using std::endl;

extern const float PI = 3.1415926535;
extern const float E = 2.718281828459045;
extern const float epsilon = 0.000001;
int hitnum = 0;

int main() {
  std::string dragon = "./data/cornell-box/cornell-box.";
  std::string veach = "./data/veach-mis/veach-mis.";
  std::string stairs = "./data/staircase/stairscase.";
  std::string model = stairs;

  Scene *scene = Importer::OBJ_import(model + "obj");

  Camera *camera = new Camera();
  Importer::XML_import(model + "xml", scene, camera);

  PathTracing pt(scene, camera, 1024, 0.4);
  pt.enable_BVH();

  pt.render();

  delete scene;
  delete camera;
}