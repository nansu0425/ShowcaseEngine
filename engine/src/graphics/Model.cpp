#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <nlohmann/json.hpp>
#include <tiny_gltf.h>

#include <showcase/graphics/Model.h>
#include <showcase/graphics/CommandList.h>
#include <showcase/core/Log.h>

namespace showcase {

void Model::Shutdown(DescriptorHeap& srvHeap) {
    for (auto& mesh : meshes) {
        for (auto& prim : mesh.primitives) {
            prim.vertexBuffer.Shutdown();
            prim.indexBuffer.Shutdown();
        }
    }
    for (auto& tex : textures) {
        tex.Shutdown(srvHeap);
    }
    meshes.clear();
    materials.clear();
    textures.clear();
}

static bool LoadTextures(const tinygltf::Model& gltfModel,
                         ID3D12Device* device, D3D12MA::Allocator* allocator,
                         CommandQueue& cmdQueue, DescriptorHeap& srvHeap,
                         std::vector<Texture>& outTextures) {
    if (gltfModel.images.empty()) return true;

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

        if (!outTextures[i].InitFromMemory(
                device, allocator, cmdList.Get(), srvHeap,
                image.image.data(),
                static_cast<uint32_t>(image.width),
                static_cast<uint32_t>(image.height),
                static_cast<uint32_t>(image.component))) {
            SE_LOG_ERROR("Failed to load texture at index {}", i);
            return false;
        }
    }

    cmdList.Close();
    cmdQueue.ExecuteCommandList(cmdList.Get());
    cmdQueue.Flush();

    for (auto& tex : outTextures) {
        tex.ReleaseUploadResources();
    }

    cmdList.Shutdown();
    return true;
}

static void LoadMaterials(const tinygltf::Model& gltfModel,
                          std::vector<Material>& outMaterials) {
    outMaterials.resize(gltfModel.materials.size());

    for (size_t i = 0; i < gltfModel.materials.size(); ++i) {
        const auto& gltfMat = gltfModel.materials[i];
        auto& mat = outMaterials[i];

        const auto& pbr = gltfMat.pbrMetallicRoughness;
        mat.baseColorFactor = DirectX::SimpleMath::Vector4(
            static_cast<float>(pbr.baseColorFactor[0]),
            static_cast<float>(pbr.baseColorFactor[1]),
            static_cast<float>(pbr.baseColorFactor[2]),
            static_cast<float>(pbr.baseColorFactor[3]));

        if (pbr.baseColorTexture.index >= 0) {
            const auto& texInfo = gltfModel.textures[pbr.baseColorTexture.index];
            mat.baseColorTextureIndex = texInfo.source;
        }
    }
}

static void ExtractVerticesAndIndices(const tinygltf::Model& gltfModel,
                                      const tinygltf::Primitive& gltfPrim,
                                      std::vector<ModelVertex>& vertices,
                                      std::vector<uint32_t>& indices) {
    // Position
    const auto posIt = gltfPrim.attributes.find("POSITION");
    if (posIt == gltfPrim.attributes.end()) return;

    const auto& posAccessor = gltfModel.accessors[posIt->second];
    const auto& posView = gltfModel.bufferViews[posAccessor.bufferView];
    const auto& posBuffer = gltfModel.buffers[posView.buffer];
    const auto* posData = reinterpret_cast<const float*>(
        posBuffer.data.data() + posView.byteOffset + posAccessor.byteOffset);

    const size_t vertexCount = posAccessor.count;
    vertices.resize(vertexCount);

    const size_t posStride = posView.byteStride ? posView.byteStride / sizeof(float) : 3;
    for (size_t i = 0; i < vertexCount; ++i) {
        vertices[i].position = DirectX::SimpleMath::Vector3(
            posData[i * posStride + 0],
            posData[i * posStride + 1],
            posData[i * posStride + 2]);
    }

    // Normal
    const auto normIt = gltfPrim.attributes.find("NORMAL");
    if (normIt != gltfPrim.attributes.end()) {
        const auto& normAccessor = gltfModel.accessors[normIt->second];
        const auto& normView = gltfModel.bufferViews[normAccessor.bufferView];
        const auto& normBuffer = gltfModel.buffers[normView.buffer];
        const auto* normData = reinterpret_cast<const float*>(
            normBuffer.data.data() + normView.byteOffset + normAccessor.byteOffset);

        const size_t normStride = normView.byteStride ? normView.byteStride / sizeof(float) : 3;
        for (size_t i = 0; i < vertexCount; ++i) {
            vertices[i].normal = DirectX::SimpleMath::Vector3(
                normData[i * normStride + 0],
                normData[i * normStride + 1],
                normData[i * normStride + 2]);
        }
    } else {
        for (size_t i = 0; i < vertexCount; ++i) {
            vertices[i].normal = DirectX::SimpleMath::Vector3(0.0f, 1.0f, 0.0f);
        }
    }

    // TexCoord
    const auto texIt = gltfPrim.attributes.find("TEXCOORD_0");
    if (texIt != gltfPrim.attributes.end()) {
        const auto& texAccessor = gltfModel.accessors[texIt->second];
        const auto& texView = gltfModel.bufferViews[texAccessor.bufferView];
        const auto& texBuffer = gltfModel.buffers[texView.buffer];
        const auto* texData = reinterpret_cast<const float*>(
            texBuffer.data.data() + texView.byteOffset + texAccessor.byteOffset);

        const size_t texStride = texView.byteStride ? texView.byteStride / sizeof(float) : 2;
        for (size_t i = 0; i < vertexCount; ++i) {
            vertices[i].texCoord = DirectX::SimpleMath::Vector2(
                texData[i * texStride + 0],
                texData[i * texStride + 1]);
        }
    } else {
        for (size_t i = 0; i < vertexCount; ++i) {
            vertices[i].texCoord = DirectX::SimpleMath::Vector2(0.0f, 0.0f);
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

static DirectX::BoundingBox ComputeAABB(const std::vector<ModelVertex>& vertices) {
    if (vertices.empty()) {
        return DirectX::BoundingBox({0, 0, 0}, {0, 0, 0});
    }

    DirectX::XMVECTOR vMin = DirectX::XMLoadFloat3(&vertices[0].position);
    DirectX::XMVECTOR vMax = vMin;

    for (size_t i = 1; i < vertices.size(); ++i) {
        DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&vertices[i].position);
        vMin = DirectX::XMVectorMin(vMin, v);
        vMax = DirectX::XMVectorMax(vMax, v);
    }

    DirectX::BoundingBox aabb;
    DirectX::BoundingBox::CreateFromPoints(aabb, vMin, vMax);
    return aabb;
}

bool ModelLoader::LoadGLTF(const std::string& filepath,
                           ID3D12Device* device, D3D12MA::Allocator* allocator,
                           CommandQueue& cmdQueue, DescriptorHeap& srvHeap,
                           Model& outModel) {
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool result;
    if (filepath.size() >= 4 && filepath.substr(filepath.size() - 4) == ".glb") {
        result = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filepath);
    } else {
        result = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filepath);
    }

    if (!warn.empty()) SE_LOG_WARN("glTF warning: {}", warn);
    if (!err.empty()) SE_LOG_ERROR("glTF error: {}", err);
    if (!result) {
        SE_LOG_ERROR("Failed to load glTF: {}", filepath);
        return false;
    }

    // Load textures
    if (!LoadTextures(gltfModel, device, allocator, cmdQueue, srvHeap, outModel.textures)) {
        return false;
    }

    // Load materials
    LoadMaterials(gltfModel, outModel.materials);

    // Load meshes
    outModel.meshes.resize(gltfModel.meshes.size());

    DirectX::XMVECTOR modelMin = DirectX::XMVectorReplicate(FLT_MAX);
    DirectX::XMVECTOR modelMax = DirectX::XMVectorReplicate(-FLT_MAX);

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

            if (vertices.empty()) continue;

            // Create vertex buffer
            const uint32_t vbSize = static_cast<uint32_t>(vertices.size() * sizeof(ModelVertex));
            if (!prim.vertexBuffer.InitAsVertexBuffer(device, allocator,
                    vertices.data(), vbSize, sizeof(ModelVertex))) {
                SE_LOG_ERROR("Failed to create vertex buffer for mesh '{}'", mesh.name);
                return false;
            }

            // Create index buffer
            prim.indexCount = static_cast<uint32_t>(indices.size());
            if (!indices.empty()) {
                const uint32_t ibSize = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
                if (!prim.indexBuffer.InitAsIndexBuffer(device, allocator,
                        indices.data(), ibSize, DXGI_FORMAT_R32_UINT)) {
                    SE_LOG_ERROR("Failed to create index buffer for mesh '{}'", mesh.name);
                    return false;
                }
            }

            prim.materialIndex = gltfPrim.material;
            prim.localAABB = ComputeAABB(vertices);

            // Expand model AABB
            DirectX::XMVECTOR primMin = DirectX::XMVectorSubtract(
                DirectX::XMLoadFloat3(&prim.localAABB.Center),
                DirectX::XMLoadFloat3(&prim.localAABB.Extents));
            DirectX::XMVECTOR primMax = DirectX::XMVectorAdd(
                DirectX::XMLoadFloat3(&prim.localAABB.Center),
                DirectX::XMLoadFloat3(&prim.localAABB.Extents));
            modelMin = DirectX::XMVectorMin(modelMin, primMin);
            modelMax = DirectX::XMVectorMax(modelMax, primMax);
        }
    }

    DirectX::BoundingBox::CreateFromPoints(outModel.localAABB, modelMin, modelMax);

    SE_LOG_INFO("Loaded glTF: {} ({} meshes, {} materials, {} textures)",
                filepath, outModel.meshes.size(), outModel.materials.size(),
                outModel.textures.size());
    return true;
}

} // namespace showcase
