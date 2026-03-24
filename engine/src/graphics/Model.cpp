#include <showcase/graphics/Model.h>

#include <showcase/core/Log.h>
#include <showcase/graphics/RenderContext.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <nlohmann/json.hpp>
#include <tiny_gltf.h>

#include <unordered_set>

namespace showcase {

// ── Lifecycle ────────────────────────────────────────────────────────
void Model::Shutdown(RenderContext& ctx) {
    std::unordered_set<Texture*> freedTextures;
    for (auto& mesh : meshes) {
        for (auto& prim : mesh.primitives) {
            prim.vertexBuffer.Shutdown();
            prim.indexBuffer.Shutdown();
            if (prim.material && prim.material->baseColorTexture) {
                Texture* tex = prim.material->baseColorTexture.get();
                if (freedTextures.insert(tex).second) {
                    tex->Shutdown(ctx.GetSrvHeap());
                }
            }
        }
    }
    meshes.clear();
}

// ── Texture loading ──────────────────────────────────────────────────
static bool LoadTextures(const tinygltf::Model& gltfModel, RenderContext& ctx,
                         std::vector<std::shared_ptr<Texture>>& outTextures) {
    if (gltfModel.images.empty())
        return true;

    auto* device = ctx.GetDevice().GetDevice();
    auto* allocator = ctx.GetDevice().GetAllocator();

    CommandList cmdList;
    if (!cmdList.Init(device, D3D12_COMMAND_LIST_TYPE_DIRECT)) {
        SE_LOG_ERROR("Failed to create command list for texture upload");
        return false;
    }
    cmdList.Reset();

    outTextures.resize(gltfModel.images.size());

    for (size_t i = 0; i < gltfModel.images.size(); ++i) {
        const auto& image = gltfModel.images[i];

        if (image.image.empty()) {
            SE_LOG_WARN("Skipping empty image at index {}", i);
            continue;
        }

        outTextures[i] = std::make_shared<Texture>();
        if (!outTextures[i]->InitFromMemory(device, allocator, cmdList.Get(), ctx.GetSrvHeap(), image.image.data(),
                                            static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height),
                                            static_cast<uint32_t>(image.component))) {
            SE_LOG_ERROR("Failed to load texture at index {}", i);
            return false;
        }
    }

    cmdList.Close();
    ctx.GetDirectQueue().ExecuteCommandList(cmdList.Get());
    ctx.GetDirectQueue().Flush();

    for (auto& tex : outTextures) {
        if (tex)
            tex->ReleaseUploadResources();
    }

    cmdList.Shutdown();
    return true;
}

// ── Material loading ─────────────────────────────────────────────────
static void LoadMaterials(const tinygltf::Model& gltfModel, const std::vector<std::shared_ptr<Texture>>& textures,
                          std::vector<std::shared_ptr<Material>>& outMaterials) {
    outMaterials.resize(gltfModel.materials.size());

    for (size_t i = 0; i < gltfModel.materials.size(); ++i) {
        const auto& gltfMat = gltfModel.materials[i];
        std::shared_ptr<Material> mat = std::make_shared<Material>();

        const auto& pbr = gltfMat.pbrMetallicRoughness;
        mat->baseColorFactor =
            Vector4(static_cast<float>(pbr.baseColorFactor[0]), static_cast<float>(pbr.baseColorFactor[1]),
                    static_cast<float>(pbr.baseColorFactor[2]), static_cast<float>(pbr.baseColorFactor[3]));

        if (pbr.baseColorTexture.index >= 0) {
            const auto& texInfo = gltfModel.textures[pbr.baseColorTexture.index];
            if (texInfo.source >= 0 && texInfo.source < static_cast<int>(textures.size())) {
                mat->baseColorTexture = textures[texInfo.source];
            }
        }

        outMaterials[i] = std::move(mat);
    }
}

// ── Mesh extraction ──────────────────────────────────────────────────
static void ExtractVerticesAndIndices(const tinygltf::Model& gltfModel, const tinygltf::Primitive& gltfPrim,
                                      std::vector<ModelVertex>& vertices, std::vector<uint32_t>& indices) {
    // Position
    const auto posIt = gltfPrim.attributes.find("POSITION");
    if (posIt == gltfPrim.attributes.end())
        return;

    const auto& posAccessor = gltfModel.accessors[posIt->second];
    const auto& posView = gltfModel.bufferViews[posAccessor.bufferView];
    const auto& posBuffer = gltfModel.buffers[posView.buffer];
    const auto* posData =
        reinterpret_cast<const float*>(posBuffer.data.data() + posView.byteOffset + posAccessor.byteOffset);

    const size_t vertexCount = posAccessor.count;
    vertices.resize(vertexCount);

    const size_t posStride = posView.byteStride ? posView.byteStride / sizeof(float) : 3;
    for (size_t i = 0; i < vertexCount; ++i) {
        vertices[i].position =
            Vector3(posData[i * posStride + 0], posData[i * posStride + 1], posData[i * posStride + 2]);
    }

    // Normal
    const auto normIt = gltfPrim.attributes.find("NORMAL");
    if (normIt != gltfPrim.attributes.end()) {
        const auto& normAccessor = gltfModel.accessors[normIt->second];
        const auto& normView = gltfModel.bufferViews[normAccessor.bufferView];
        const auto& normBuffer = gltfModel.buffers[normView.buffer];
        const auto* normData =
            reinterpret_cast<const float*>(normBuffer.data.data() + normView.byteOffset + normAccessor.byteOffset);

        const size_t normStride = normView.byteStride ? normView.byteStride / sizeof(float) : 3;
        for (size_t i = 0; i < vertexCount; ++i) {
            vertices[i].normal =
                Vector3(normData[i * normStride + 0], normData[i * normStride + 1], normData[i * normStride + 2]);
        }
    } else {
        for (size_t i = 0; i < vertexCount; ++i) {
            vertices[i].normal = Vector3(0.0f, 1.0f, 0.0f);
        }
    }

    // TexCoord
    const auto texIt = gltfPrim.attributes.find("TEXCOORD_0");
    if (texIt != gltfPrim.attributes.end()) {
        const auto& texAccessor = gltfModel.accessors[texIt->second];
        const auto& texView = gltfModel.bufferViews[texAccessor.bufferView];
        const auto& texBuffer = gltfModel.buffers[texView.buffer];
        const auto* texData =
            reinterpret_cast<const float*>(texBuffer.data.data() + texView.byteOffset + texAccessor.byteOffset);

        const size_t texStride = texView.byteStride ? texView.byteStride / sizeof(float) : 2;
        for (size_t i = 0; i < vertexCount; ++i) {
            vertices[i].texCoord = Vector2(texData[i * texStride + 0], texData[i * texStride + 1]);
        }
    } else {
        for (size_t i = 0; i < vertexCount; ++i) {
            vertices[i].texCoord = Vector2(0.0f, 0.0f);
        }
    }

    // Indices
    if (gltfPrim.indices >= 0) {
        const auto& idxAccessor = gltfModel.accessors[gltfPrim.indices];
        const auto& idxView = gltfModel.bufferViews[idxAccessor.bufferView];
        const auto& idxBuffer = gltfModel.buffers[idxView.buffer];
        const uint8_t* idxData = idxBuffer.data.data() + idxView.byteOffset + idxAccessor.byteOffset;

        indices.resize(idxAccessor.count);

        if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const auto* src = reinterpret_cast<const uint16_t*>(idxData);
            for (size_t i = 0; i < idxAccessor.count; ++i) {
                indices[i] = static_cast<uint32_t>(src[i]);
            }
        } else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            memcpy(indices.data(), idxData, idxAccessor.count * sizeof(uint32_t));
        } else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            const auto* src = idxData;
            for (size_t i = 0; i < idxAccessor.count; ++i) {
                indices[i] = static_cast<uint32_t>(src[i]);
            }
        }
    }
}

// ── AABB computation ─────────────────────────────────────────────────
static BoundingBox ComputeLocalAABB(const std::vector<ModelVertex>& vertices) {
    if (vertices.empty()) {
        return BoundingBox(Vector3(0.f, 0.f, 0.f), Vector3(0.f, 0.f, 0.f));
    }

    Vector3 vMin = vertices[0].position;
    Vector3 vMax = vMin;

    for (size_t i = 1; i < vertices.size(); ++i) {
        vMin = Vector3::Min(vMin, vertices[i].position);
        vMax = Vector3::Max(vMax, vertices[i].position);
    }

    return CreateAABB(vMin, vMax);
}

// ── glTF loading ─────────────────────────────────────────────────────
bool ModelLoader::LoadGLTF(RenderContext& ctx, const std::string& filepath, Model& outModel) {
    auto* device = ctx.GetDevice().GetDevice();
    auto* allocator = ctx.GetDevice().GetAllocator();
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool result;
    if (filepath.size() >= 4 && filepath.substr(filepath.size() - 4) == ".glb") {
        result = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filepath);
    } else {
        result = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filepath);
    }

    if (!warn.empty())
        SE_LOG_WARN("glTF warning: {}", warn);
    if (!err.empty())
        SE_LOG_ERROR("glTF error: {}", err);
    if (!result) {
        SE_LOG_ERROR("Failed to load glTF: {}", filepath);
        return false;
    }

    // Load textures and materials
    std::vector<std::shared_ptr<Texture>> textures;
    if (!LoadTextures(gltfModel, ctx, textures)) {
        return false;
    }

    std::vector<std::shared_ptr<Material>> materials;
    LoadMaterials(gltfModel, textures, materials);

    // Load meshes
    outModel.meshes.resize(gltfModel.meshes.size());

    Vector3 modelMin(FLT_MAX, FLT_MAX, FLT_MAX);
    Vector3 modelMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (size_t mi = 0; mi < gltfModel.meshes.size(); ++mi) {
        const auto& gltfMesh = gltfModel.meshes[mi];
        auto& mesh = outModel.meshes[mi];
        mesh.name = gltfMesh.name;
        mesh.primitives.resize(gltfMesh.primitives.size());

        for (size_t pi = 0; pi < gltfMesh.primitives.size(); ++pi) {
            const auto& gltfPrim = gltfMesh.primitives[pi];
            auto& prim = mesh.primitives[pi];

            std::vector<ModelVertex> vertices;
            std::vector<uint32_t> indices;
            ExtractVerticesAndIndices(gltfModel, gltfPrim, vertices, indices);

            if (vertices.empty())
                continue;

            // Create vertex buffer
            const uint32_t vbSize = static_cast<uint32_t>(vertices.size() * sizeof(ModelVertex));
            if (!prim.vertexBuffer.InitAsVertexBuffer(device, allocator, vertices.data(), vbSize,
                                                      sizeof(ModelVertex))) {
                SE_LOG_ERROR("Failed to create vertex buffer for mesh '{}'", mesh.name);
                return false;
            }

            // Create index buffer
            prim.indexCount = static_cast<uint32_t>(indices.size());
            if (!indices.empty()) {
                const uint32_t ibSize = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
                if (!prim.indexBuffer.InitAsIndexBuffer(device, allocator, indices.data(), ibSize,
                                                        DXGI_FORMAT_R32_UINT)) {
                    SE_LOG_ERROR("Failed to create index buffer for mesh '{}'", mesh.name);
                    return false;
                }
            }

            if (gltfPrim.material >= 0 && gltfPrim.material < static_cast<int>(materials.size())) {
                prim.material = materials[gltfPrim.material];
            }
            prim.localAABB = ComputeLocalAABB(vertices);

            // Expand model AABB
            Vector3 center(prim.localAABB.Center);
            Vector3 extents(prim.localAABB.Extents);
            modelMin = Vector3::Min(modelMin, center - extents);
            modelMax = Vector3::Max(modelMax, center + extents);
        }
    }

    outModel.localAABB = CreateAABB(modelMin, modelMax);

    SE_LOG_INFO("Loaded glTF: {} ({} meshes, {} materials, {} textures)", filepath, outModel.meshes.size(),
                materials.size(), textures.size());
    return true;
}

} // namespace showcase
