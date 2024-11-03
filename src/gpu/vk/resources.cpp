#include "resources.h"
#include "util.h"
#include "device.h"
#include "configuration.h"
#include "gpu/core/enum_class_flags.h"
#include <iostream>

Texture::Texture(Device &InDevice, const RHITextureCreateDesc &InCreateDesc, VkImage InImage, bool bUnused)
    : RHITexture(InCreateDesc), device(&InDevice), Image(InImage), MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
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