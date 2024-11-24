#pragma once
#include "gpu/core/math/vec.h"
#include "gpu/engine/classes/materials/materials.h"
#include "gpu/definitions.h"
#include <vector>
#include <memory>

/// @brief  A set of static mesh triangles which are rendered with the same material.
struct StaticMeshSection
{
    /** The index of the material with which to render this section. */
    int32 materialIndex;

    /** Range of vertices and indices used when rendering this section. */
    uint32 firstIndex;
    uint32 numTriangles;
};

class StaticMeshLODResources
{
public:
    std::vector<StaticMeshSection> sections;

    std::vector<Vec3> position;
    std::vector<Vec3> tangent;
    std::vector<Vec3> normal;
    std::vector<Vec2> uv;
    std::vector<uint32> index;
};