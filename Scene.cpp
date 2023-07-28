#include "scene.h"

#include <iostream>
#include <stdexcept>
#include <unordered_map>

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "glm/glm.hpp"

extern const float epsilon;

Triangle::Triangle(std::array<const Vertex *, 3> &_v, const Material *_mat)
    : v(_v), material(_mat) {
  normal = v[0]->normal + v[1]->normal + v[2]->normal;
  normal /= 3;
  normal = glm::normalize(normal);
}

const Material *Triangle::getMaterial() const { return material; }

float Triangle::getArea() const {
  glm::vec3 a = v[1]->position - v[0]->position;
  glm::vec3 b = v[2]->position - v[0]->position;
  glm::vec3 c = glm::cross(a, b);
  return sqrtf(glm::dot(c, c)) / 2;
}

glm::vec3 Triangle::getRandomPoint() const {
  glm::vec3 a = v[1]->position - v[0]->position;
  glm::vec3 b = v[2]->position - v[0]->position;
  int temp = rand() % 1000;
  float p = (float)temp / 1000.0f;
  temp = rand() % 1000;
  float q = (float)temp / 1000.0f;
  return v[0]->position * (1 - sqrtf(p)) + v[1]->position * sqrtf(p) * (1 - q) +
         v[2]->position * sqrtf(p) * q;
}

Mesh::Mesh(std::vector<Vertex> &_vertices, std::vector<unsigned> &_indices,
           unsigned _matIds)
    : vertices(_vertices), indices(_indices), materialIndex(_matIds) {}

void Scene::load(std::string fileName) {
  Assimp::Importer importer;
  // And have it read the given file with some example postprocessing
  // Usually - if speed is not the most important aspect for you - you'll
  // probably to request more postprocessing than we do in this example.
  const aiScene *scene = importer.ReadFile(
      fileName, aiProcess_CalcTangentSpace | aiProcess_Triangulate |
                    aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);

  // If the import failed, report it
  if (nullptr == scene) {
    throw std::exception();
  }
  std::cout << "Number of meshes is " << scene->mNumMeshes << std::endl;
  std::cout << "Number of materials is " << scene->mNumMaterials << std::endl;
  std::cout << "Number of textures is " << scene->mNumTextures << std::endl;
  this->meshes.resize(0);
  this->materials.resize(0);

  for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
    unsigned numVertices = scene->mMeshes[i]->mNumVertices;
    std::vector<Vertex> vertices(numVertices);
    for (unsigned j = 0; j < numVertices; ++j) {
      vertices[j].position = {scene->mMeshes[i]->mVertices[j].x,
                              scene->mMeshes[i]->mVertices[j].y,
                              scene->mMeshes[i]->mVertices[j].z};
      vertices[j].normal = {scene->mMeshes[i]->mNormals[j].x,
                            scene->mMeshes[i]->mNormals[j].y,
                            scene->mMeshes[i]->mNormals[j].z};
      vertices[j].texCoord = {scene->mMeshes[i]->mTextureCoords[0][j].x,
                              scene->mMeshes[i]->mTextureCoords[0][j].y};
    }

    unsigned numTriangle = scene->mMeshes[i]->mNumFaces;
    std::vector<unsigned> indices(numTriangle * 3);
    for (unsigned j = 0; j < numTriangle; ++j) {
      assert(scene->mMeshes[i]->mFaces[j].mNumIndices == 3);
      indices[j * 3 + 0] = scene->mMeshes[i]->mFaces[j].mIndices[0];
      indices[j * 3 + 1] = scene->mMeshes[i]->mFaces[j].mIndices[1];
      indices[j * 3 + 2] = scene->mMeshes[i]->mFaces[j].mIndices[2];
    }
    this->meshes.push_back(
        Mesh(vertices, indices, scene->mMeshes[i]->mMaterialIndex));
  }
  for (unsigned i = 0; i < scene->mNumMaterials; ++i) {
    Material m;
    m.name = scene->mMaterials[i]->GetName().C_Str();
    aiColor3D color(0.f, 0.f, 0.f);
    scene->mMaterials[i]->Get(AI_MATKEY_COLOR_DIFFUSE, color);
    m.albedo = {color.r, color.g, color.b};
    scene->mMaterials[i]->Get(AI_MATKEY_COLOR_AMBIENT, color);
    m.ambient = {color.r, color.g, color.b};
    scene->mMaterials[i]->Get(AI_MATKEY_COLOR_EMISSIVE, color);
    m.emissive = {color.r, color.g, color.b};
    this->materials.push_back(m);
  }
}

std::vector<Triangle> Scene::getAllFaces() {
  size_t faceCount = 0;
  std::vector<Triangle> ret;
  for (auto &e : meshes) {
    unsigned numFaces = e.getNumFaces();
    const std::vector<Vertex> &vertices = e.getVertices();
    const std::vector<unsigned> &indices = e.getIndices();
    for (unsigned i = 0; i < numFaces; ++i) {
      std::array<const Vertex *, 3> v;
      v[0] = &vertices[indices[i * 3 + 0]];
      v[1] = &vertices[indices[i * 3 + 1]];
      v[2] = &vertices[indices[i * 3 + 2]];
      Material *mat = &materials[e.getMaterialIndex()];
      ret.push_back(Triangle(v, mat));
      ret[ret.size() - 1].id = faceCount++;
    }
  }
  return std::move(ret);
}