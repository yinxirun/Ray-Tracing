#pragma once
#include "gpu/core/math/vec.h"
#include <string>

enum class MaterialType { DIFFUSE, SPECULAR, EMISSIVE };
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