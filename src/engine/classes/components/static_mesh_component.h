#pragma once

#include "mesh_component.h"
#include "engine/classes/static_mesh.h"
#include <memory>

/**
 * StaticMeshComponent is used to create an instance of a UStaticMesh.
 * A static mesh is a piece of geometry that consists of a static set of polygons.
 */
class StaticMeshComponent : public MeshComponent
{
private:
    std::shared_ptr<StaticMesh> staticMesh;

public:
    virtual bool SetStaticMesh(std::shared_ptr<StaticMesh> NewMesh);
};