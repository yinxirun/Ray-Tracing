#ifndef _IMPORTER_H
#define _IMPORTER_H
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Camera.h"
#include "Material.h"
#include "Mesh.h"
#include "Scene.h"
#include "Vec.h"
#include "tinyxml2.h"  ///< open source from github, aimed to load XML file.

//const unsigned BUFFER_SIZE = 67108864;
const unsigned BUFFER_SIZE = 512;

class Importer {
 public:
  /// @brief get the directory where the file locates.
  static std::string directory(const std::string);
  static std::vector<std::string> split(const char *, char);

  /// @brief load materials from .mtl file
  static void MTL_import(const std::string, Scene *scene);

  /// @brief load mesh from .obj file (and .mtl file associated if exists)
  static Scene *OBJ_import(const std::string);

  /// @brief load information about light and camera from XML file.
  static void XML_import(const std::string, Scene *scene, Camera *camera);

  static void OBJ_import(const std::string filepath, Scene *scene);
};
#endif