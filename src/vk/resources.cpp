#include "resources.h"
#include "util.h"
#include "device.h"
#include "configuration.h"
#include "core/enum_class_flags.h"
#include <iostream>

VulkanTexture::VulkanTexture(Device &InDevice, const TextureCreateDesc &InCreateDesc, VkImage InImage, bool bUnused)
    : Texture(InCreateDesc), device(&InDevice), Image(InImage), MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
      ImageOwnerType(EImageOwnerType::ExternalOwner)
{
}

VulkanTexture::~VulkanTexture()
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