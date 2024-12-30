#pragma once
#include "core/math/vec.h"
#include "RHI/RHIResources.h"

#include <memory>

struct CameraInfo
{
    Mat4 model;
    Mat4 view;
    Mat4 proj;
};

struct SimpleVertex
{
    Vec3 position;
    Vec3 normal;
    Vec2 texCoord;

    bool operator==(const SimpleVertex &other) const
    {
        return position == other.position && normal == other.normal &&
               texCoord == other.texCoord;
    }
};

namespace std
{
    template <>
    struct hash<SimpleVertex>
    {
        size_t operator()(SimpleVertex const &vertex) const
        {
            return ((hash<glm::vec3>()(vertex.position) ^
                     (hash<glm::vec3>()(vertex.normal) << 1)) >>
                    1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

class SimpleMaterial
{
public:
    enum class MaterialType
    {
        DIFFUSE,
        SPECULAR,
        EMISSIVE
    } type;

    Vec3 albedo;
    Vec3 emission;
    float roughness = 0;
    float metallic = 0;
    std::string name;
};

/// @brief A set of static mesh triangles which are rendered with the same material.
class SimpleSection
{
public:
    /** The index of the material with which to render this section. */
    int32 materialIndex;
    /** Range of vertices and indices used when rendering this section. */
    uint32 firstIndex;
    uint32 numTriangles;
};

class SimpleLODResource
{
public:
    std::vector<SimpleSection> sections;
    std::vector<Vec3> position;
    std::vector<Vec3> tangent;
    std::vector<Vec3> normal;
    std::vector<Vec2> uv;
    std::vector<uint32> index;

    std::shared_ptr<Buffer> positonBuffer;
    std::shared_ptr<Buffer> tangentBuffer;
    std::shared_ptr<Buffer> normalBuffer;
    std::shared_ptr<Buffer> uvBuffer;
    std::shared_ptr<Buffer> indexBuffer;
};

class SimpleStaticMesh
{
public:
    std::vector<SimpleLODResource> LOD;
    std::vector<SimpleMaterial> materials;
};

class SimpleScene
{
public:
    void AddStaticMesh(const SimpleStaticMesh &mesh)
    {
        staticMeshes.push_back(std::move(mesh));
    }
    
    std::vector<SimpleStaticMesh> staticMeshes;
};
