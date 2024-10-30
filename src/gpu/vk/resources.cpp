#include "resources.h"

Texture::Texture(Device &InDevice, const RHITextureCreateDesc &InCreateDesc, VkImage InImage, bool bUnused)
    : RHITexture(InCreateDesc),device(&InDevice), Image(InImage), MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
     ImageOwnerType(EImageOwnerType::ExternalOwner)
{
    
}

Texture::~Texture()
{
    // VULKAN_TRACK_OBJECT_DELETE(FVulkanTexture, this);
    if (ImageOwnerType != EImageOwnerType::Aliased)
    {
        if (PartialView != DefaultView)
        {
            delete PartialView;
        }

        delete DefaultView;
        DestroySurface();
    }
}

void Texture::DestroySurface()
{
}