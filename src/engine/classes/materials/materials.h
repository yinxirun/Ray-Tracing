#pragma once
#include "core/math/vec.h"
#include <string>

enum class MaterialType : uint16
{
    DIFFUSE,
    SPECULAR,
    EMISSIVE,
    NUM_MATERIAL
};

struct MetallicRoughnessParameters
{
    Vec3 albedo;
    float roughness;
    float metallic;
};

class Material
{
public:
    std::string name;
    glm::vec3 albedo;
    glm::vec3 emission;
    float roughness = 0;
    float metallic = 0;
    MaterialType type;
};