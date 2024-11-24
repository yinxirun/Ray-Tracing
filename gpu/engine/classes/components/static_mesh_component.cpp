#include "static_mesh_component.h"

bool StaticMeshComponent::SetStaticMesh(std::shared_ptr<StaticMesh> NewMesh)
{
    // Do nothing if we are already using the supplied static mesh
    if (NewMesh == staticMesh)
    {
        return false;
    }
    staticMesh = NewMesh;

    return true;
}