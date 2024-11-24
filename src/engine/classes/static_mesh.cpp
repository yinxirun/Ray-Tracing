#include "engine/classes/materials/materials.h"
#include "static_mesh.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <iostream>
#include <unordered_map>
struct Vertex
{
    Vec3 position;
    Vec3 normal;
    Vec2 texCoord;

    bool operator==(const Vertex &other) const
    {
        return position == other.position && normal == other.normal &&
               texCoord == other.texCoord;
    }
};

namespace std
{
    template <>
    struct hash<Vertex>
    {
        size_t operator()(Vertex const &vertex) const
        {
            return ((hash<glm::vec3>()(vertex.position) ^
                     (hash<glm::vec3>()(vertex.normal) << 1)) >>
                    1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}
void load_wavefront_static_mesh(std::string inputfile, StaticMesh &staticMesh)
{
    tinyobj::ObjReaderConfig reader_config;

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(inputfile, reader_config))
    {
        if (!reader.Error().empty())
        {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        exit(1);
    }

    if (!reader.Warning().empty())
    {
        std::cout << "TinyObjReader: " << reader.Warning();
    }

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    auto &mats = reader.GetMaterials();

    assert(shapes.size() == 1);

    for (size_t s = 0; s < shapes.size(); s++)
    {
        StaticMeshLODResources &currentLOD = staticMesh.lod1;
        assert(currentLOD.index.empty());
        assert(currentLOD.position.empty());
        assert(currentLOD.normal.empty());
        assert(currentLOD.uv.empty());
        assert(currentLOD.tangent.empty());

        size_t numPolygon = shapes[s].mesh.num_face_vertices.size();

        // 1. load vertices and polygon. Remove unnecessary vertices.
        std::vector<uint32> indices;
        {
            std::unordered_map<Vertex, size_t> uniqueVertices{};
            std::vector<Vertex> vertices;

            size_t index_offset = 0;
            for (size_t f = 0; f < numPolygon; f++)
            {
                size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

                // only support triangles
                assert(fv == 3);

                // Loop over vertices in the face.
                for (size_t v = 0; v < fv; v++)
                {
                    Vertex vertex;

                    // access to vertex
                    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                    vertex.position.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                    vertex.position.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                    vertex.position.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                    // Check if `normal_index` is zero or positive. negative = no normal
                    // data
                    if (idx.normal_index >= 0)
                    {
                        vertex.normal.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
                        vertex.normal.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
                        vertex.normal.z = attrib.normals[3 * size_t(idx.normal_index) + 2];
                    }

                    // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                    if (idx.texcoord_index >= 0)
                    {
                        vertex.texCoord.x = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                        vertex.texCoord.y = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                    }
                    // 去除重复顶点
                    if (uniqueVertices.count(vertex) == 0)
                    {
                        uniqueVertices[vertex] = vertices.size();
                        vertices.push_back(vertex);
                    }
                    indices.push_back(uniqueVertices[vertex]);
                }
                index_offset += fv;
            }
            // 各个属性分开存储
            for (Vertex &v : vertices)
            {
                currentLOD.position.push_back(v.position);
                currentLOD.normal.push_back(v.normal);
                currentLOD.uv.push_back(v.texCoord);
            }
        }
        // 2. Load materials
        {
            staticMesh.materials.resize(mats.size());
            for (size_t i = 0; i < mats.size(); ++i)
            {
                std::shared_ptr<Material> &material = staticMesh.materials[i];
                material = std::make_shared<Material>();
                material->name = mats[i].name;
                material->albedo =
                    glm::vec3(mats[i].diffuse[0], mats[i].diffuse[1], mats[i].diffuse[2]);
                material->emission = glm::vec3(mats[i].emission[0], mats[i].emission[1],
                                               mats[i].emission[2]);
                material->metallic = mats[i].metallic;
                material->roughness = mats[i].roughness;

                if (material->emission.x > 0 || material->emission.y > 0 ||
                    material->emission.z > 0)
                {
                    material->type = MaterialType::EMISSIVE;
                }
                else if (material->metallic > 0 || material->roughness > 0)
                {
                    material->type = MaterialType::SPECULAR;
                }
                else
                {
                    material->type = MaterialType::DIFFUSE;
                }
            }
        }
        // 3. process sections
        {
            // map material ID to section ID
            std::unordered_map<size_t, size_t> mat2Section;
            std::vector<std::vector<uint32>> section2Polygons;

            for (size_t f = 0; f < numPolygon; f++)
            {
                if (mat2Section.count(shapes[s].mesh.material_ids[f]) == 0)
                {
                    size_t sectionID = currentLOD.sections.size();
                    currentLOD.sections.push_back(StaticMeshSection());
                    section2Polygons.push_back(std::vector<uint32>());
                    currentLOD.sections[sectionID].materialIndex = shapes[s].mesh.material_ids[f];
                    section2Polygons[sectionID].push_back(f);
                    mat2Section[shapes[s].mesh.material_ids[f]] = sectionID;
                }
                else
                {
                    size_t sectionID = mat2Section[shapes[s].mesh.material_ids[f]];
                    section2Polygons[sectionID].push_back(f);
                }
            }

            // 重排index，使得相同材质的polygon挨在一起
            for (int sectionID = 0; sectionID < section2Polygons.size(); ++sectionID)
            {
                auto &polygonIDs = section2Polygons[sectionID];

                currentLOD.sections[sectionID].firstIndex = currentLOD.index.size();
                currentLOD.sections[sectionID].numTriangles = polygonIDs.size();
                for (auto polygonID : polygonIDs)
                {
                    size_t fv = size_t(shapes[s].mesh.num_face_vertices[polygonID]);
                    assert(fv == 3);

                    currentLOD.index.push_back(indices[polygonID * fv + 0]);
                    currentLOD.index.push_back(indices[polygonID * fv + 1]);
                    currentLOD.index.push_back(indices[polygonID * fv + 2]);
                }
            }
        }
    }
}