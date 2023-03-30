#include "Importer.h"

#include <thread>

void Importer::MTL_import(const std::string filepath, Scene *scene) {
  std::fstream file(filepath, std::ios::in);
  Material *mtl;
  while (!file.eof()) {
    char data[BUFFER_SIZE];
    file.getline(data, BUFFER_SIZE);
    std::vector<std::string> items = split(data, ' ');
    unsigned l = strlen(data);
    if (items.size() == 0) continue;
    std::string keyword = items[0];
    if (keyword.compare("newmtl") == 0) {
      mtl = new Material();
      scene->materials.emplace_back(mtl);
      mtl->name = items[1];
      mtl->id = scene->materials.size() - 1;
    } else if (keyword.compare("Ks") == 0) {
      mtl->specular.x = (float)atof(items[1].c_str());
      mtl->specular.y = (float)atof(items[2].c_str());
      mtl->specular.z = (float)atof(items[3].c_str());
    } else if (keyword.compare("Ka") == 0) {
      mtl->ambient.x = (float)atof(items[1].c_str());
      mtl->ambient.y = (float)atof(items[2].c_str());
      mtl->ambient.z = (float)atof(items[3].c_str());
    } else if (keyword.compare("Kd") == 0) {
      mtl->diffuse.x = (float)atof(items[1].c_str());
      mtl->diffuse.y = (float)atof(items[2].c_str());
      mtl->diffuse.z = (float)atof(items[3].c_str());
    } else if (keyword.compare("Tr") == 0) {
      mtl->transparency.x = (float)atof(items[1].c_str());
      mtl->transparency.y = (float)atof(items[2].c_str());
      mtl->transparency.z = (float)atof(items[3].c_str());
    } else if (keyword.compare("Ns") == 0) {
      mtl->shineness = (float)atoi(items[1].c_str());
    } else if (keyword.compare("Ni") == 0) {
      mtl->refraction = (float)atoi(items[1].c_str());
    } else if (keyword.compare("map_Kd") == 0) {
      mtl->bind_map(directory(filepath) + items[1]);
    } else {
      std::cout << keyword << " is unknown. From MTL. " << std::endl;
    }
  }
  file.close();
  std::cout << "Material Loaded. " << std::endl;
}

std::string Importer::directory(const std::string filepath) {
  std::string ret;
  int l = filepath.length();
  for (int i = l - 1; i >= -1; i--) {
    if (filepath[i] == '/' || i < 0) {
      ret = filepath.substr(0, i + 1);
      break;
    }
  }
  return ret;
}

std::vector<std::string> Importer::split(const char *str, char c = ' ') {
  std::vector<std::string> ret;
  int p = 0;
  int l = strlen(str);
  std::string buffer;
  while (p <= l) {
    // 使用布尔短路是一个好主意吗？
    if (p == l || str[p] == c) {
      if (buffer.length() != 0) {
        ret.emplace_back(buffer);
        buffer = "";
      }
    } else
      buffer.push_back(str[p]);
    p += 1;
  }
  return ret;
}

Scene *Importer::OBJ_import(const std::string filepath) {
  Scene *scene = new Scene();

  Material *material_current = nullptr;
  Attribute &attr = scene->mesh->get_attr();

  std::fstream file(filepath, std::ios::in);
  while (!file.eof()) {
    char data[BUFFER_SIZE];
    file.getline(data, BUFFER_SIZE);
    std::vector<std::string> items = split(data);

    if (items.size() == 0) continue;

    std::string keyword = items[0];
    if (keyword.compare("v") == 0) {
      float a = (float)atof(items[1].c_str());
      float b = (float)atof(items[2].c_str());
      float c = (float)atof(items[3].c_str());
      attr.add_vertex(Vector3D(a, b, c));
    } else if (keyword.compare("vn") == 0) {
      float a = (float)atof(items[1].c_str());
      float b = (float)atof(items[2].c_str());
      float c = (float)atof(items[3].c_str());
      attr.add_normal(Vector3D(a, b, c));
    } else if (keyword.compare("vt") == 0) {
      float a = (float)atof(items[1].c_str());
      float b = (float)atof(items[2].c_str());
      float c = 0;
      attr.add_texcoord(Vector3D(a, b, c));
    } else if (keyword.compare("f") == 0) {
      Triangle face_temp;
      for (int i = 1; i < items.size(); ++i) {
        std::vector<std::string> temp = split(items[i].c_str(), '/');
        face_temp.vertices.emplace_back(atoi(temp[0].c_str()) - 1);
        face_temp.normals.emplace_back(atoi(temp[2].c_str()) - 1);
        face_temp.texcords.emplace_back(atoi(temp[1].c_str()) - 1);
      }
      face_temp.material = material_current;
      scene->mesh->add_face(face_temp);
    } else if (keyword.compare("g") == 0) {
    } else if (keyword.compare("usemtl") == 0) {
      Material *m = scene->find_material(items[1]);
      if (m != nullptr) material_current = m;
    } else if (keyword.compare("mtllib") == 0) {
      std::string name = items[1];
      // obtain the path of .mtl file that locates in the same directory as .obj
      // file.
      name = directory(filepath) + name;
      // handle .mtl file
      MTL_import(name, scene);
    } else {
      std::cout << keyword << " is unknown. " << std::endl;
    }
  }
  file.close();
  std::cout << "Mesh Loaded. " << std::endl;
  return scene;
}

void Importer::XML_import(const std::string filepath, Scene *scene,
                          Camera *camera) {
  if (scene == nullptr || camera == nullptr) {
    std::cout << "scene and camera can not be null pointer." << std::endl;
    return;
  }
  tinyxml2::XMLDocument *doc = new tinyxml2::XMLDocument();
  doc->LoadFile(filepath.c_str());

  // handle camera's parameter
  tinyxml2::XMLElement *c = doc->FirstChildElement();
  const char *attr = c->Attribute("type");
  if (strcmp(attr, "perspective") == 0)
    camera->set_type(projection_type::perspective);
  else
    std::cout << "ERROR! From Importer::XML_import " << std::endl;
  int width = atoi(c->Attribute("width"));
  int height = atoi(c->Attribute("height"));
  camera->set_image_size(width, height);
  camera->set_fov(atof(c->Attribute("fovy")));

  // handle camera's orientation
  tinyxml2::XMLElement *p = c->FirstChildElement();
  for (int i = 0; i < 3; i++) {
    Vector3D v(atof(p->Attribute("x")), atof(p->Attribute("y")),
               atof(p->Attribute("z")));
    if (strcmp(p->Name(), "eye") == 0)
      camera->set_location(v);
    else if (strcmp(p->Name(), "lookat") == 0) {
      v.normalize();
      camera->set_lookat(v);
    } else if (strcmp(p->Name(), "up") == 0) {
      v.normalize();
      camera->set_up(v);
    } else
      std::cout << "ERROR! From Importer::XML_import " << std::endl;
    p = p->NextSiblingElement();
  }

  // handle information about light sources
  int num_lights = 0;
  c = c->NextSiblingElement();
  while (c != nullptr) {
    AreaLight *light = new AreaLight();
    std::string mtlname(c->Attribute("mtlname"));
    std::vector<std::string> radiance = split(c->Attribute("radiance"), ',');
    light->radiance.x = atof(radiance[0].c_str());
    light->radiance.y = atof(radiance[1].c_str());
    light->radiance.z = atof(radiance[2].c_str());
    // check if exists
    Material *m = scene->find_material(mtlname);
    if (m == nullptr) {
      delete m;
      continue;
      std::cout << "This type of light is not existing. " << std::endl;
    }
    m->emission.x = light->radiance.z;
    m->emission.y = light->radiance.y;
    m->emission.z = light->radiance.x;

    // capture all face index with which faces applies this material.
    unsigned num_faces = scene->mesh->num_faces();
    std::vector<Triangle> &faces = scene->mesh->get_faces();
    for (unsigned i = 0; i < num_faces; ++i)
      if (faces[i].material == m) light->faces_id.emplace_back(i);
    num_lights += light->faces_id.size();
    scene->light_sources.emplace_back(light);
    c = c->NextSiblingElement();
  }
  std::cout << num_lights << " area lights in the scene. " << std::endl;
}

void Importer::OBJ_import(const std::string filepath, Scene *scene) {
  if (scene == nullptr) {
    std::cout << "Error: scene is a null pointer. " << std::endl;
    return;
  }
  // 读入
  const unsigned SIZE = 67108864;
  char *buffer = new char[SIZE];
  std::fstream file(filepath, std::ios::in);
  file.read(buffer, SIZE);
  int byte_num = file.gcount();
  buffer[byte_num] = '\0';
  file.close();

  Material *material_current = nullptr;
  Attribute &attr = scene->mesh->get_attr();

  char word[128];
  int p = 0;
  while (p < byte_num) {
    for (int i = 0;; ++i) {
      if (buffer[p + i] == '\n' || buffer[p + i] == '\0') {
        p = p + i + 1;
        word[i] = '\0';
        break;
      }
      word[i] = buffer[p + i];
    }
    std::vector<std::string> items = split(word);
    if (items.size() == 0) continue;
    std::string keyword = items[0];
    if (keyword.compare("v") == 0) {
      float a = (float)atof(items[1].c_str());
      float b = (float)atof(items[2].c_str());
      float c = (float)atof(items[3].c_str());
      attr.add_vertex(Vector3D(a, b, c));
    } else if (keyword.compare("vn") == 0) {
      float a = (float)atof(items[1].c_str());
      float b = (float)atof(items[2].c_str());
      float c = (float)atof(items[3].c_str());
      attr.add_normal(Vector3D(a, b, c));
    } else if (keyword.compare("vt") == 0) {
      float a = (float)atof(items[1].c_str());
      float b = (float)atof(items[2].c_str());
      float c = 0;
      attr.add_texcoord(Vector3D(a, b, c));
    } else if (keyword.compare("f") == 0) {
      Triangle face_temp;
      for (int i = 1; i < items.size(); ++i) {
        std::vector<std::string> temp = split(items[i].c_str(), '/');
        face_temp.vertices.emplace_back(atoi(temp[0].c_str()) - 1);
        face_temp.normals.emplace_back(atoi(temp[2].c_str()) - 1);
        face_temp.texcords.emplace_back(atoi(temp[1].c_str()) - 1);
      }
      face_temp.material = material_current;
      scene->mesh->add_face(face_temp);
    } else if (keyword.compare("g") == 0) {
    } else if (keyword.compare("usemtl") == 0) {
      Material *m = scene->find_material(items[1]);
      if (m != nullptr) material_current = m;
    } else if (keyword.compare("mtllib") == 0) {
      std::string name = items[1];
      // obtain the path of .mtl file that locates in the same directory as .obj
      // file.
      name = directory(filepath) + name;
      // handle .mtl file
      MTL_import(name, scene);
    } else {
      std::cout << keyword << " is unknown. From OBJ. " << std::endl;
    }
  }
  delete[] buffer;
}
