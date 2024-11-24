#pragma once
#include "core/math/vec.h"
#include "engine/classes/materials/materials.h"
#include "engine/static_mesh_resources.h"
#include "definitions.h"
#include <vector>
#include <memory>
#include <fstream>

/**
 * A StaticMesh is a piece of geometry that consists of a static set of polygons.
 * Static Meshes can be translated, rotated, and scaled, but they cannot have their vertices animated in any way. As such, they are more efficient
 * to render than other types of geometry such as USkeletalMesh, and they are often the basic building block of levels created in the engine.
 */
class StaticMesh
{
public:
    StaticMeshLODResources lod1;
    std::vector<std::shared_ptr<Material>> materials;
};