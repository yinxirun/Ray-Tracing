#include "RHIResources.h"
#include "RHIDefinitions.h"

RHIViewableResource::RHIViewableResource(ERHIResourceType InResourceType, ERHIAccess InAccess)
{
}

RHITexture::RHITexture(const RHITextureCreateDesc &InDesc) : RHIViewableResource(RRT_Texture, InDesc.InitialState)
{
}