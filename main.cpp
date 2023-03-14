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
extern const float epsilon = 0.0001;
int hitnum = 0;

void convert_to_binary();
void load(Scene *scene);

void test() {
  int w = 1024, h = 1024;
  std::fstream f("veach-mis", std::ios::in | std::ios::binary);
  float *img = new float[w * h * 3];
  f.read((char *)img, w * h * 3 * sizeof(float));
  unsigned char *rgb = new unsigned char[w * h * 3];

  float average = 0;
  float max=100;
  for (int i = 0; i < w * h * 3; ++i) {
    if(img[i]>max) img[i]=max;
    average += img[i];
  }
  average /= (w * h * 3);
  float k = 0.7;
  for (int i = 0; i < h * w * 3; i += 1) {
    float r = img[i] / (img[i] + average / k);
    rgb[i] = (unsigned char)(255.0f * r);
  }

  int bmi[] = {
      3 * w * h + 54, 0, 54, 40, w, h, 1 | 3 * 8 << 16, 0, 0, 0, 0, 0, 0};
  FILE *fp = fopen("test.bmp", "wb");
  fprintf(fp, "BM");
  fwrite(&bmi, 52, 1, fp);
  fwrite(rgb, 1, 3 * w * h, fp);
  fclose(fp);
  delete[] rgb;

  delete img;
  f.close();
}

int main() {

  std::string dragon = "./data/cornell-box/cornell-box.";
  std::string veach = "./data/veach-mis/veach-mis.";

  Scene *scene = Importer::OBJ_import(dragon + "obj");
  Camera *camera = new Camera();
  Importer::XML_import(dragon + "xml", scene, camera);
  std::cout << "Model Loaded. " << std::endl;
  PathTracing pt(scene, camera, 256, 0.4);
  pt.enable_BVH();
  clock_t start = clock();
  pt.render();
  clock_t end = clock();
  std::cout << "TIME    " << end - start << std::endl;
  std::cout << hitnum << std::endl;
  camera->save_BMP("debug.bmp");
  delete scene;
  delete camera;
  /*
    clock_t start = clock();
    Scene *scene = new Scene();
    load(scene);
  */

  // debug
  // {
  //   std::vector<Face> faces_new;
  //   int l=scene->mesh->get_faces().size();
  //   for(int i=0;i<l;i+=77){
  //     faces_new.emplace_back(scene->mesh->get_faces()[i]);
  //   }
  //   scene->mesh->faces=faces_new;
  // }

  /*
    Camera *camera = new Camera();
    Importer::XML_import("./data/cornell-box/cornell-box.xml", scene, camera);
    clock_t end = clock();
    std::cout << "Loading the model costs " << end - start << " ms" <<
    std::endl;

    PathTracing pt(scene, camera, 10, 0.5);
    start = clock();
    pt.enable_BVH();
    end = clock();
    std::cout << "Constructing BVH costs " << end - start << " ms" << std::endl;

    start = clock();
    pt.render();
    end = clock();
    std::cout << "Rendering costs " << end - start << " ms" << std::endl;
    std::cout << hitnum << std::endl;
    camera->save_BMP("debug.bmp");
    delete scene;
    delete camera;*/
}

void convert_to_binary() {
  Scene *scene = new Scene();
  Importer::OBJ_import("./data/cornell-box/cornell-box.obj", scene);

  std::fstream f("dragon.dat",
                 std::ios::out | std::ios::trunc | std::ios::binary);

  unsigned n = scene->mesh->num_vertices();
  float *buffer = new float[n * 3];
  for (int i = 0; i < n; ++i) {
    buffer[i * 3] = scene->mesh->get_attr().vertices[i].x;
    buffer[i * 3 + 1] = scene->mesh->get_attr().vertices[i].y;
    buffer[i * 3 + 2] = scene->mesh->get_attr().vertices[i].z;
  }
  f.write((char *)&n, sizeof(unsigned));
  f.write((char *)buffer, n * 3 * sizeof(float));
  delete[] buffer;

  n = scene->mesh->num_normals();
  buffer = new float[n * 3];
  for (int i = 0; i < n; ++i) {
    buffer[i * 3] = scene->mesh->get_attr().normals[i].x;
    buffer[i * 3 + 1] = scene->mesh->get_attr().normals[i].y;
    buffer[i * 3 + 2] = scene->mesh->get_attr().normals[i].z;
  }
  f.write((char *)&n, sizeof(unsigned));
  f.write((char *)buffer, n * 3 * sizeof(float));
  delete[] buffer;

  n = scene->mesh->num_texcoords();
  buffer = new float[n * 3];
  for (int i = 0; i < n; ++i) {
    buffer[i * 3] = scene->mesh->get_attr().texcoords[i].x;
    buffer[i * 3 + 1] = scene->mesh->get_attr().texcoords[i].y;
    buffer[i * 3 + 2] = scene->mesh->get_attr().texcoords[i].z;
  }
  f.write((char *)&n, sizeof(unsigned));
  f.write((char *)buffer, n * 3 * sizeof(float));
  delete[] buffer;

  n = scene->mesh->num_faces();
  unsigned *ubuffer = new unsigned[n * 10];
  for (int i = 0; i < n; ++i) {
    ubuffer[i * 10] = scene->mesh->get_faces()[i].vertices[0];
    ubuffer[i * 10 + 1] = scene->mesh->get_faces()[i].normals[0];
    ubuffer[i * 10 + 2] = scene->mesh->get_faces()[i].texcords[0];
    ubuffer[i * 10 + 3] = scene->mesh->get_faces()[i].vertices[1];
    ubuffer[i * 10 + 4] = scene->mesh->get_faces()[i].normals[1];
    ubuffer[i * 10 + 5] = scene->mesh->get_faces()[i].texcords[1];
    ubuffer[i * 10 + 6] = scene->mesh->get_faces()[i].vertices[2];
    ubuffer[i * 10 + 7] = scene->mesh->get_faces()[i].normals[2];
    ubuffer[i * 10 + 8] = scene->mesh->get_faces()[i].texcords[2];
    ubuffer[i * 10 + 9] = scene->mesh->get_faces()[i].material->id;
  }
  f.write((char *)&n, sizeof(unsigned));
  f.write((char *)ubuffer, n * 10 * sizeof(unsigned));
  delete ubuffer;

  f.close();
  delete scene;
}
void load(Scene *scene) {
  if (scene == nullptr) return;
  Importer::MTL_import("./data/cornell-box/cornell-box.mtl", scene);
  std::fstream f("dragon.dat", std::ios::in | std::ios::binary);
  float *fbuffer;
  unsigned *ubuffer;
  Attribute &attr = scene->mesh->get_attr();
  unsigned n;

  f.read((char *)&n, sizeof(unsigned));
  fbuffer = new float[n * 3];
  f.read((char *)fbuffer, n * 3 * sizeof(float));
  for (int i = 0; i < n; i++) {
    Vector3D vertex(fbuffer[i * 3], fbuffer[i * 3 + 1], fbuffer[i * 3 + 2]);
    attr.add_vertex(vertex);
  }
  delete[] fbuffer;

  f.read((char *)&n, sizeof(unsigned));
  fbuffer = new float[n * 3];
  f.read((char *)fbuffer, n * 3 * sizeof(float));
  for (int i = 0; i < n; i++) {
    Vector3D normal(fbuffer[i * 3], fbuffer[i * 3 + 1], fbuffer[i * 3 + 2]);
    attr.add_normal(normal);
  }
  delete[] fbuffer;

  f.read((char *)&n, sizeof(unsigned));
  fbuffer = new float[n * 3];
  f.read((char *)fbuffer, n * 3 * sizeof(float));
  for (int i = 0; i < n; i++) {
    Vector3D texcoord(fbuffer[i * 3], fbuffer[i * 3 + 1], fbuffer[i * 3 + 2]);
    attr.add_texcoord(texcoord);
  }
  delete[] fbuffer;

  f.read((char *)&n, sizeof(unsigned));
  ubuffer = new unsigned[n * 10];
  f.read((char *)ubuffer, n * 10 * sizeof(unsigned));
  for (int i = 0; i < n; ++i) {
    Triangle face;
    face.vertices.emplace_back(ubuffer[0]);
    face.vertices.emplace_back(ubuffer[3]);
    face.vertices.emplace_back(ubuffer[6]);
    face.normals.emplace_back(ubuffer[1]);
    face.normals.emplace_back(ubuffer[4]);
    face.normals.emplace_back(ubuffer[7]);
    face.texcords.emplace_back(ubuffer[2]);
    face.texcords.emplace_back(ubuffer[5]);
    face.texcords.emplace_back(ubuffer[8]);
    face.material = scene->materials[ubuffer[9]];
    face.set_attr(&attr);
    face.init();
    scene->mesh->add_face(face);
  }
  delete[] ubuffer;

  f.close();
}