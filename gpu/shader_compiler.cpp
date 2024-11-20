#include <spirv_cross/spirv_glsl.hpp>
#include <spirv_cross/spirv_common.hpp>
#include <string>
#include <fstream>
#include <iostream>
#include "definitions.h"
#include "gpu/core/serialization/memory_writer.h"
#include "gpu/vk/shader_resources.h"
#include "gpu/RHI/RHIDefinitions.h"

std::string func(std::vector<uint8_t> &&inCode)
{
    // Read SPIR-V from disk or similar.
    std::vector<uint32_t> spirv_binary(inCode.size() * sizeof(uint8_t) / sizeof(uint32_t));
    memcpy(spirv_binary.data(), inCode.data(), inCode.size());

    spirv_cross::CompilerGLSL glsl(std::move(spirv_binary));

    // The SPIR-V is now parsed, and we can perform reflection on it.
    spirv_cross::ShaderResources resources = glsl.get_shader_resources();

    // Get all sampled images in the shader.
    for (auto &resource : resources.stage_inputs)
    {
        unsigned set = 0;
        unsigned binding = glsl.get_decoration(resource.id, spv::DecorationLocation);
        printf("Image %s at set = %u, binding = %u\n", resource.name.c_str(), set, binding);

        // Modify the decoration to prepare it for GLSL.
        glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);

        // Some arbitrary remapping if we want.
        glsl.set_decoration(resource.id, spv::DecorationBinding, set * 16 + binding);
    }

    // Set some options.
    // spirv_cross::CompilerGLSL::Options options;
    // options.version = 310;
    // options.es = false;
    // glsl.set_common_options(options);

    // Compile to GLSL, ready to give to GL driver.
    std::string source = glsl.compile();

    return source;
}

UniformBufferBaseType ParseType(spirv_cross::SPIRType type)
{
    using namespace spirv_cross;
    switch (type.basetype)
    {
    case SPIRType::Float:
        return UniformBufferBaseType::UBMT_FLOAT32;
        break;

    default:
        exit(1);
        break;
    }
}

std::vector<uint8> LoadShader(std::string spvFilename, ShaderFrequency freq)
{
    std::ifstream file(spvFilename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<uint8_t> buffer(fileSize);
    file.seekg(0);
    file.read((char *)buffer.data(), fileSize);
    file.close();

    // Read SPIR-V from disk or similar.
    std::vector<uint32_t> spirv_binary(buffer.size() * sizeof(uint8_t) / sizeof(uint32_t));
    memcpy(spirv_binary.data(), buffer.data(), buffer.size());

    spirv_cross::CompilerGLSL glsl(std::move(spirv_binary));

    // The SPIR-V is now parsed, and we can perform reflection on it.
    spirv_cross::ShaderResources resources = glsl.get_shader_resources();

    ShaderHeader header;
    header.InOutMask = 0;
    if (freq == ShaderFrequency::SF_Vertex)
    {
        // Vertex Input
        for (auto &res : resources.stage_inputs)
        {
            unsigned attribLocation = glsl.get_decoration(res.id, spv::DecorationLocation);
            header.InOutMask |= (1 << attribLocation);
        }

        // Uniform Buffer
        for (auto &res : resources.uniform_buffers)
        {
            ShaderHeader::UniformBufferInfo ubInfo;

            uint32 binding = glsl.get_decoration(res.id, spv::DecorationBinding);
            uint32 set = glsl.get_decoration(res.id, spv::DecorationDescriptorSet);
            ubInfo.LayoutHash = (set << 8) | binding;

            spirv_cross::SPIRType type = glsl.get_type(res.base_type_id);

            uint32_t member_count = type.member_types.size();
            for (int i = 0; i < member_count; i++)
            {
                ShaderHeader::UBResourceInfo ubResInfo;

                std::string resourceName = glsl.get_member_name(res.base_type_id, i);

                const spirv_cross::SPIRType &member_type = glsl.get_type(type.member_types[i]);

                ubResInfo.UBBaseType = ParseType(member_type);

                size_t resourceSize = glsl.get_declared_struct_member_size(type, i);
                uint32 resourceOffset = glsl.type_struct_member_offset(type, i);

                ubInfo.ResourceEntries.push_back(ubResInfo);
            }

            header.UniformBuffers.push_back(ubInfo);
        }
    }
    else if (freq == ShaderFrequency::SF_Pixel)
    {
        for (auto &res : resources.stage_outputs)
        {
            unsigned attribLocation = glsl.get_decoration(res.id, spv::DecorationLocation);
            header.InOutMask |= (1 << attribLocation);
        }
    }

    std::vector<uint8> data;
    MemoryWriter ar(data, true);
    ar << header;

    uint32 spirvCodeSizeBytes = buffer.size();
    ar << spirvCodeSizeBytes;
    ar.Serialize(buffer.data(), spirvCodeSizeBytes);

    return data;
}
