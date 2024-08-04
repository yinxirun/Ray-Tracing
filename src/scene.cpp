#include "scene.h"

#include <iostream>
#include <stdexcept>
#include <unordered_map>
#ifdef ASSIMP
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#else
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#endif
#include "geometry/primitive.h"
#include "geometry/triangle.h"
#include "geometry/vertex.h"
#include "glm/glm.hpp"
#include "path_tracing.h"

std::vector<std::shared_ptr<Primitive>> Scene::AllTriangles() {
  std::vector<std::shared_ptr<Primitive>> primitives;
  size_t triangleCount = 0;

  for (auto &mesh : meshes) {
    const std::vector<Vertex> &vertices = mesh.getVertices();
    const std::vector<unsigned> &indices = mesh.getIndices();

    for (size_t i = 0; i < mesh.sections.size(); ++i) {
      Section &section = mesh.sections[i];
      Material *mat = materials[section.materialId].get();

      for (auto polygonID : section.polygonIDs) {
        size_t vertexInstanceID[3] = {polygonID * 3, polygonID * 3 + 1,
                                      polygonID * 3 + 2};

        size_t vertexID[3] = {indices[vertexInstanceID[0]],
                              indices[vertexInstanceID[1]],
                              indices[vertexInstanceID[2]]};

        std::array<const Vertex *, 3> v;
        v[0] = &vertices[vertexID[0]];
        v[1] = &vertices[vertexID[1]];
        v[2] = &vertices[vertexID[2]];

        auto t = std::make_shared<Triangle>(v, mat);
        t->SetID(triangleCount++);
        primitives.push_back(t);
      }
    }
  }
  return primitives;
}

void Scene::load(std::string inputfile) {
#ifdef ASSIMP
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
#else
  tinyobj::ObjReaderConfig reader_config;

  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(inputfile, reader_config)) {
    if (!reader.Error().empty()) {
      std::cerr << "TinyObjReader: " << reader.Error();
    }
    exit(1);
  }

  if (!reader.Warning().empty()) {
    std::cout << "TinyObjReader: " << reader.Warning();
  }

  auto &attrib = reader.GetAttrib();
  auto &shapes = reader.GetShapes();
  auto &mats = reader.GetMaterials();

  for (size_t s = 0; s < shapes.size(); s++) {
    Mesh mesh;

    size_t numPolygon = shapes[s].mesh.num_face_vertices.size();

    // map material ID to section ID
    std::unordered_map<size_t, size_t> mat2Section;

    // handle sections
    for (size_t f = 0; f < numPolygon; f++) {
      if (mat2Section.count(shapes[s].mesh.material_ids[f]) == 0) {
        size_t sectionID = mesh.sections.size();
        mesh.sections.push_back(Section());
        mesh.sections[sectionID].materialId = shapes[s].mesh.material_ids[f];
        mesh.sections[sectionID].polygonIDs.push_back(f);
        mat2Section[shapes[s].mesh.material_ids[f]] = sectionID;
      } else {
        size_t sectionID = mat2Section[shapes[s].mesh.material_ids[f]];
        mesh.sections[sectionID].polygonIDs.push_back(f);
      }
    }

    // load vertices and polygon. Remove unnecessary vertices.
    std::unordered_map<Vertex, size_t> uniqueVertices{};

    size_t index_offset = 0;
    for (size_t f = 0; f < numPolygon; f++) {
      size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

      // only support triangles
      assert(fv == 3);

      // Loop over vertices in the face.
      for (size_t v = 0; v < fv; v++) {
        Vertex vertex;

        // access to vertex
        tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
        vertex.position.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
        vertex.position.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
        vertex.position.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

        // Check if `normal_index` is zero or positive. negative = no normal
        // data
        if (idx.normal_index >= 0) {
          vertex.normal.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
          vertex.normal.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
          vertex.normal.z = attrib.normals[3 * size_t(idx.normal_index) + 2];
        }

        // Check if `texcoord_index` is zero or positive. negative = no texcoord
        // data
        if (idx.texcoord_index >= 0) {
          vertex.texCoord.x =
              attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
          vertex.texCoord.y =
              attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
        }

        if (uniqueVertices.count(vertex) == 0) {
          uniqueVertices[vertex] = mesh.vertices.size();
          mesh.vertices.push_back(vertex);
        }
        mesh.indices.push_back(uniqueVertices[vertex]);
      }
      index_offset += fv;
    }

    this->meshes.push_back(std::move(mesh));
  }

  // Load materials
  materials.resize(mats.size());
  for (size_t i = 0; i < mats.size(); ++i) {
    materials[i] = std::make_unique<Material>();
    materials[i]->name = mats[i].name;
    materials[i]->albedo =
        glm::vec3(mats[i].diffuse[0], mats[i].diffuse[1], mats[i].diffuse[2]);
    materials[i]->emission = glm::vec3(mats[i].emission[0], mats[i].emission[1],
                                       mats[i].emission[2]);

    if (materials[i]->emission.x > 0 || materials[i]->emission.y > 0 ||
        materials[i]->emission.z > 0) {
      materials[i]->type = MaterialType::EMISSIVE;
    } else {
      materials[i]->type = MaterialType::DIFFUSE;
    }
  }

#endif
}