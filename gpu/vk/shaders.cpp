#include <vector>
#include <memory>
#include "gpu/core/misc/crc.h"
#include "resources.h"
#include "device.h"
#include "rhi.h"

template <typename ShaderType>
ShaderType *VulkanShaderFactory::CreateShader(std::vector<uint8> &Code, Device *Device)
{
    const uint32 ShaderCodeLen = Code.size();
    const uint32 ShaderCodeCRC = MemCrc32(Code.data(), Code.size());
    const uint64 ShaderKey = ((uint64)ShaderCodeLen | ((uint64)ShaderCodeCRC << 32));

    ShaderType *RetShader = new ShaderType(Device);
    RetShader->Setup(std::move(Code), ShaderKey);
    return RetShader;
}

VertexShader *RHI::CreateVertexShader(std::vector<uint8> Code)
{
    VertexShader *shader = device->GetShaderFactory().CreateShader<VulkanVertexShader>(Code, device);
    return shader;
}

PixelShader *RHI::CreatePixelShader(std::vector<uint8> Code)
{
    PixelShader *shader = device->GetShaderFactory().CreateShader<VulkanPixelShader>(Code, device);
    return shader;
}

VulkanShader::~VulkanShader() {}

void VulkanShader::Setup(std::vector<uint8> &&spirv, uint64 shaderKey)
{
    this->spirv = std::move(spirv);
    this->ShaderKey = std::move(shaderKey);
}
