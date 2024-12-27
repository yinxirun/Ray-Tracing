#include "application.h"
#include "definitions.h"
#include "core/math/vec.h"
#include "RHI/RHICommandList.h"
#include "RHI/RHIResources.h"
#include "RHI/RHIContext.h"
#include "RHI/dynamic_rhi.h"

#include "GLFW/glfw3.h"
#include "RHI/dynamic_rhi.h"
#include "RHI/RHI.h"
#include "RHI/pipeline_state_cache.h"
#include "render_core/static_states.h"
#include "vk/viewport.h"

#include "stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <memory>

struct SimpleVertex
{
    Vec3 position;
    Vec3 normal;
    Vec2 texCoord;

    bool operator==(const SimpleVertex &other) const
    {
        return position == other.position && normal == other.normal &&
               texCoord == other.texCoord;
    }
};

namespace std
{
    template <>
    struct hash<SimpleVertex>
    {
        size_t operator()(SimpleVertex const &vertex) const
        {
            return ((hash<glm::vec3>()(vertex.position) ^
                     (hash<glm::vec3>()(vertex.normal) << 1)) >>
                    1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

class SimpleMaterial
{
public:
    enum class MaterialType
    {
        DIFFUSE,
        SPECULAR,
        EMISSIVE
    } type;

    Vec3 albedo;
    Vec3 emission;
    float roughness = 0;
    float metallic = 0;
    std::string name;
};

/// @brief A set of static mesh triangles which are rendered with the same material.
class SimpleSection
{
public:
    /** The index of the material with which to render this section. */
    int32 materialIndex;
    /** Range of vertices and indices used when rendering this section. */
    uint32 firstIndex;
    uint32 numTriangles;
};

class SimpleLODResource
{
public:
    std::vector<SimpleSection> sections;
    std::vector<Vec3> position;
    std::vector<Vec3> tangent;
    std::vector<Vec3> normal;
    std::vector<Vec2> uv;
    std::vector<uint32> index;

    std::shared_ptr<Buffer> positonBuffer;
    std::shared_ptr<Buffer> tangentBuffer;
    std::shared_ptr<Buffer> normalBuffer;
    std::shared_ptr<Buffer> uvBuffer;
    std::shared_ptr<Buffer> indexBuffer;
};

class SimpleStaticMesh
{
public:
    std::vector<SimpleLODResource> LOD;
    std::vector<SimpleMaterial> materials;
};

class SimpleScene
{
public:
    void AddStaticMesh(const SimpleStaticMesh &mesh)
    {
        staticMeshes.push_back(std::move(mesh));
    }

    void Render()
    {
    }

    std::vector<SimpleStaticMesh> staticMeshes;
};

SimpleStaticMesh LoadWavefrontStaticMesh(std::string inputfile, RHICommandListBase &cmdList)
{
    SimpleStaticMesh staticMesh;

    tinyobj::ObjReaderConfig reader_config;
    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(inputfile, reader_config))
    {
        if (!reader.Error().empty())
        {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        exit(1);
    }

    if (!reader.Warning().empty())
    {
        std::cout << "TinyObjReader: " << reader.Warning();
    }

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    auto &mats = reader.GetMaterials();

    assert(shapes.size() == 1);

    staticMesh.LOD.resize(shapes.size());
    for (size_t s = 0; s < shapes.size(); s++)
    {
        SimpleLODResource &currentLOD = staticMesh.LOD[0];
        assert(currentLOD.index.empty());
        assert(currentLOD.position.empty());
        assert(currentLOD.normal.empty());
        assert(currentLOD.uv.empty());
        assert(currentLOD.tangent.empty());

        size_t numPolygon = shapes[s].mesh.num_face_vertices.size();

        // 1. load vertices and polygon. Remove unnecessary vertices.
        std::vector<uint32> indices;
        {
            std::unordered_map<SimpleVertex, size_t> uniqueVertices{};
            std::vector<SimpleVertex> vertices;

            size_t index_offset = 0;
            for (size_t f = 0; f < numPolygon; f++)
            {
                size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

                // only support triangles
                assert(fv == 3);

                // Loop over vertices in the face.
                for (size_t v = 0; v < fv; v++)
                {
                    SimpleVertex vertex;

                    // access to vertex
                    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                    vertex.position.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                    vertex.position.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                    vertex.position.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                    // Check if `normal_index` is zero or positive. negative = no normal
                    // data
                    if (idx.normal_index >= 0)
                    {
                        vertex.normal.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
                        vertex.normal.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
                        vertex.normal.z = attrib.normals[3 * size_t(idx.normal_index) + 2];
                    }

                    // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                    if (idx.texcoord_index >= 0)
                    {
                        vertex.texCoord.x = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                        vertex.texCoord.y = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                    }
                    // 去除重复顶点
                    if (uniqueVertices.count(vertex) == 0)
                    {
                        uniqueVertices[vertex] = vertices.size();
                        vertices.push_back(vertex);
                    }
                    indices.push_back(uniqueVertices[vertex]);
                }
                index_offset += fv;
            }
            // 各个属性分开存储
            for (SimpleVertex &v : vertices)
            {
                currentLOD.position.push_back(v.position);
                currentLOD.normal.push_back(v.normal);
                currentLOD.uv.push_back(v.texCoord);
            }
        }
        // 2. Load materials
        {
            staticMesh.materials.resize(mats.size());
            for (size_t i = 0; i < mats.size(); ++i)
            {
                SimpleMaterial &material = staticMesh.materials[i];
                material.name = mats[i].name;
                material.albedo =
                    Vec3(mats[i].diffuse[0], mats[i].diffuse[1], mats[i].diffuse[2]);
                material.emission = glm::vec3(mats[i].emission[0], mats[i].emission[1],
                                              mats[i].emission[2]);
                material.metallic = mats[i].metallic;
                material.roughness = mats[i].roughness;

                if (material.emission.x > 0 || material.emission.y > 0 ||
                    material.emission.z > 0)
                {
                    material.type = SimpleMaterial::MaterialType::EMISSIVE;
                }
                else if (material.metallic > 0 || material.roughness > 0)
                {
                    material.type = SimpleMaterial::MaterialType::SPECULAR;
                }
                else
                {
                    material.type = SimpleMaterial::MaterialType::DIFFUSE;
                }
            }
        }
        // 3. process sections
        {
            // map material ID to section ID
            std::unordered_map<size_t, size_t> mat2Section;
            std::vector<std::vector<uint32>> section2Polygons;

            for (size_t f = 0; f < numPolygon; f++)
            {
                if (mat2Section.count(shapes[s].mesh.material_ids[f]) == 0)
                {
                    size_t sectionID = currentLOD.sections.size();
                    currentLOD.sections.push_back(SimpleSection());
                    section2Polygons.push_back(std::vector<uint32>());
                    currentLOD.sections[sectionID].materialIndex = shapes[s].mesh.material_ids[f];
                    section2Polygons[sectionID].push_back(f);
                    mat2Section[shapes[s].mesh.material_ids[f]] = sectionID;
                }
                else
                {
                    size_t sectionID = mat2Section[shapes[s].mesh.material_ids[f]];
                    section2Polygons[sectionID].push_back(f);
                }
            }

            // 重排index，使得相同材质的polygon挨在一起
            for (int sectionID = 0; sectionID < section2Polygons.size(); ++sectionID)
            {
                auto &polygonIDs = section2Polygons[sectionID];

                currentLOD.sections[sectionID].firstIndex = currentLOD.index.size();
                currentLOD.sections[sectionID].numTriangles = polygonIDs.size();
                for (auto polygonID : polygonIDs)
                {
                    size_t fv = size_t(shapes[s].mesh.num_face_vertices[polygonID]);
                    assert(fv == 3);

                    currentLOD.index.push_back(indices[polygonID * fv + 0]);
                    currentLOD.index.push_back(indices[polygonID * fv + 1]);
                    currentLOD.index.push_back(indices[polygonID * fv + 2]);
                }
            }
        }

        ResourceCreateInfo ci{};
        uint32 size = currentLOD.position.size() * sizeof(Vec3);
        BufferDesc bufferDesc(size, 12, BUF_VertexBuffer);
        currentLOD.positonBuffer = CreateBuffer(bufferDesc, Access::VertexOrIndexBuffer, ci);
        void *mapped = LockBuffer_BottomOfPipe(cmdList, currentLOD.positonBuffer.get(), 0, size, ResourceLockMode::RLM_WriteOnly);
        memcpy(mapped, currentLOD.position.data(), size);
        UnlockBuffer_BottomOfPipe(cmdList, currentLOD.positonBuffer.get());

        size = currentLOD.index.size() * sizeof(uint32);
        bufferDesc = BufferDesc(size, 4, BUF_IndexBuffer);
        currentLOD.indexBuffer = CreateBuffer(bufferDesc, Access::VertexOrIndexBuffer, ci);
        mapped = LockBuffer_BottomOfPipe(cmdList, currentLOD.indexBuffer.get(), 0, size, ResourceLockMode::RLM_WriteOnly);
        memcpy(mapped, currentLOD.index.data(), size);
        UnlockBuffer_BottomOfPipe(cmdList, currentLOD.indexBuffer.get());
    }

    return staticMesh;
}

static const int WIDTH = 1600;
static const int HEIGHT = 1200;
static VulkanViewport *drawingViewport;
extern std::vector<uint8> process_shader(std::string filename, ShaderFrequency freq);

struct CameraInfo
{
    Mat4 model;
    Mat4 view;
    Mat4 proj;
};

void RunSimpleApplication(GLFWwindow *window)
{
    RHICommandListBase &immediate = RHICommandListExecutor::GetImmediateCommandList();
    immediate.SwitchPipeline(RHIPipeline::Graphics);

    SimpleScene scene;
    auto staticMesh = LoadWavefrontStaticMesh("assets/cornell-box/cornell-box_normal.obj", immediate);
    scene.AddStaticMesh(staticMesh);

    // 准备资源
    CommandContext *context = GetDefaultContext();
    std::shared_ptr<VulkanViewport> viewport = CreateViewport(window, WIDTH, HEIGHT, false, PixelFormat::PF_B8G8R8A8);
    drawingViewport = viewport.get();

    std::array<uint32, 12> indices = {4, 5, 6, 4, 6, 7,
                                      0, 1, 2, 0, 2, 3};
    std::array<float, 24> position = {0.5, -0.5, 0.5, 0.5, 0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5,
                                      0.5, -0.5, 0.6, 0.5, 0.5, 0.6, -0.5, 0.5, 0.6, -0.5, -0.5, 0.6};
    std::array<float, 24> color = {1.0f, 0.0f, 0.0f, 0.f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1, 1, 1,
                                   1.0f, 0.0f, 0.0f, 1.f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1, 0, 0};
    std::array<float, 16> uv = {0, -0, 0, 1, 1, 0, 1, 1,
                                0, -0, 0, 1, 1, 0, 1, 1};

    BufferDesc desc(position.size() * sizeof(float), sizeof(float), BufferUsageFlags::VertexBuffer);
    ResourceCreateInfo ci{};
    auto positionBuffer = CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);
    auto colorBuffer = CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);

    desc = BufferDesc(uv.size() * sizeof(float), sizeof(float), BufferUsageFlags::VertexBuffer);
    auto texCoordBuffer = CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);

    desc = BufferDesc(indices.size() * sizeof(uint32), sizeof(uint32), BufferUsageFlags::IndexBuffer);
    auto indexBuffer = CreateBuffer(desc, Access::CopyDest | Access::VertexOrIndexBuffer, ci);

    void *mapped;

    mapped = LockBuffer_BottomOfPipe(immediate, indexBuffer.get(), 0, indexBuffer->GetSize(), ResourceLockMode::RLM_WriteOnly);
    memcpy(mapped, indices.data(), indices.size() * sizeof(uint32));
    UnlockBuffer_BottomOfPipe(immediate, indexBuffer.get());

    mapped = LockBuffer_BottomOfPipe(immediate, positionBuffer.get(), 0, positionBuffer->GetSize(), ResourceLockMode::RLM_WriteOnly);
    memcpy(mapped, position.data(), position.size() * sizeof(float));
    UnlockBuffer_BottomOfPipe(immediate, positionBuffer.get());

    mapped = LockBuffer_BottomOfPipe(immediate, colorBuffer.get(), 0, colorBuffer->GetSize(), ResourceLockMode::RLM_WriteOnly);
    memcpy(mapped, color.data(), color.size() * sizeof(float));
    UnlockBuffer_BottomOfPipe(immediate, colorBuffer.get());

    mapped = LockBuffer_BottomOfPipe(immediate, texCoordBuffer.get(), 0, texCoordBuffer->GetSize(), ResourceLockMode::RLM_WriteOnly);
    memcpy(mapped, uv.data(), uv.size() * sizeof(float));
    UnlockBuffer_BottomOfPipe(immediate, texCoordBuffer.get());

    // 创建PSO
    GraphicsPipelineStateInitializer graphicsPSOInit;

    graphicsPSOInit.RenderTargetsEnabled = 1;
    graphicsPSOInit.RenderTargetFormats[0] = PF_B8G8R8A8;
    graphicsPSOInit.RenderTargetFlags[0] = TextureCreateFlags::RenderTargetable;
    graphicsPSOInit.NumSamples = 1;

    graphicsPSOInit.DepthStencilTargetFormat = PF_D24;

    graphicsPSOInit.SubpassHint = SubpassHint::None;
    graphicsPSOInit.SubpassIndex = 0;

    graphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI().get();
    graphicsPSOInit.DepthStencilState = TStaticDepthStencilState<>::GetRHI().get();

    graphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI().get();
    graphicsPSOInit.PrimitiveType = PT_TriangleList;

    VertexDeclarationElementList elements;
    elements.push_back(VertexElement(0, 0, VET_Float3, 0, 12));
    elements.push_back(VertexElement(1, 0, VET_Float3, 1, 12));
    elements.push_back(VertexElement(2, 0, VET_Float2, 2, 8));
    graphicsPSOInit.BoundShaderState.VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(elements);
    graphicsPSOInit.BoundShaderState.VertexShaderRHI = CreateVertexShader(process_shader("shaders/a.vert.spv", SF_Vertex));
    graphicsPSOInit.BoundShaderState.PixelShaderRHI = CreatePixelShader(process_shader("shaders/a.frag.spv", SF_Pixel));

    // 相机参数
    CameraInfo perCamera;
    perCamera.model = Rotate(Mat4(1), Radians(90), Vec3(0, 0, 1));
    perCamera.view = Mat4(1);
    perCamera.proj = Mat4(1);
    UniformBufferLayoutInitializer UBInit;
    UBInit.BindingFlags = UniformBufferBindingFlags::Shader;
    UBInit.ConstantBufferSize = sizeof(CameraInfo);
    UBInit.Resources.push_back({offsetof(CameraInfo, model), UniformBufferBaseType::UBMT_FLOAT32});
    UBInit.Resources.push_back({offsetof(CameraInfo, view), UniformBufferBaseType::UBMT_FLOAT32});
    UBInit.Resources.push_back({offsetof(CameraInfo, proj), UniformBufferBaseType::UBMT_FLOAT32});
    UBInit.ComputeHash();
    auto UBLayout = std::make_shared<const UniformBufferLayout>(UBInit);
    auto ub = CreateUniformBuffer(0, UBLayout, UniformBufferUsage::UniformBuffer_MultiFrame, UniformBufferValidation::None);
    rhi->UpdateUniformBuffer(immediate, ub.get(), &perCamera);
    context->SubmitCommandsHint();

    // 纹理
    int x, y, comp;
    stbi_uc *sourceData = stbi_load("assets/viking_room.png", &x, &y, &comp, STBI_rgb_alpha);
    for (int i = 0; i < x * y; ++i)
    {
        std::swap(sourceData[i * STBI_rgb_alpha + 0], sourceData[i * STBI_rgb_alpha + 2]);
    }
    TextureCreateDesc texDesc = TextureCreateDesc::Create2D("BaseColor", x, y, PF_B8G8R8A8)
                                    .SetInitialState(Access::SRVGraphics)
                                    .SetFlags(TexCreate_SRGB | TexCreate_ShaderResource);
    auto tex = CreateTexture(immediate, texDesc);
    UpdateTextureRegion2D region(0, 0, 0, 0, x, y);
    UpdateTexture2D(immediate, tex.get(), 0, region, x * STBI_rgb_alpha, sourceData);
    stbi_image_free(sourceData);
    SamplerStateInitializer samplerInit(SF_Point);
    auto sampler = CreateSamplerState(samplerInit);

    // 深度图
    texDesc.SetFormat(PF_D24)
        .SetInitialState(Access::DSVRead | Access::DSVWrite)
        .SetFlags(TexCreate_DepthStencilTargetable)
        .SetExtent(WIDTH, HEIGHT)
        .SetClearValue(ClearValueBinding(0, 0));
    auto depth = CreateTexture(immediate, texDesc);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        context->BeginDrawingViewport(viewport);
        context->BeginFrame();

        RenderPassInfo RPInfo(viewport->GetBackBuffer().get(), RenderTargetActions::Clear_Store,
                              depth.get(), EDepthStencilTargetActions::ClearDepthStencil_DontStoreDepthStencil);
        context->BeginRenderPass(RPInfo, "no name");

        // PSO的回收还没写，目前是会泄漏的
        auto *pso = CreateGraphicsPipelineState(graphicsPSOInit);
        context->SetGraphicsPipelineState(pso, 0, false);
        context->SetShaderUniformBuffer(graphicsPSOInit.BoundShaderState.VertexShaderRHI, 0, ub.get());
        context->SetShaderTexture(graphicsPSOInit.BoundShaderState.PixelShaderRHI, 0, tex.get());
        context->SetShaderSampler(graphicsPSOInit.BoundShaderState.PixelShaderRHI, 0, sampler.get());

        context->SetStreamSource(0, positionBuffer.get(), 0);
        context->SetStreamSource(1, colorBuffer.get(), 0);
        context->SetStreamSource(2, texCoordBuffer.get(), 0);
        context->DrawIndexedPrimitive(indexBuffer.get(), 0, 0, 8, 0, 4, 1);

        context->EndRenderPass();

        context->EndFrame();
        context->EndDrawingViewport(viewport.get(), false);
    }
    context->SubmitCommandsHint();
}