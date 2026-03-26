#pragma once

#include <showcase/graphics/Model.h>

#include <string>
#include <vector>

namespace showcase {

class RenderContext;

// Data collected during glTF loading, used to write the binary cache.
struct ModelCacheWriteData {
    const std::string* gltfPath = nullptr;

    // Image pixel data (raw RGBA, pre-expanded)
    struct ImageEntry {
        uint32_t width = 0;
        uint32_t height = 0;
        const uint8_t* data = nullptr; // pointer into tinygltf image buffer
        uint64_t dataSize = 0;
    };
    std::vector<ImageEntry> images;

    // Materials referencing images by index
    struct MaterialEntry {
        float baseColorFactor[4] = {1.f, 1.f, 1.f, 1.f};
        int32_t baseColorTextureIndex = -1;
        uint32_t alphaMode = 0; // 0=Opaque, 1=Mask, 2=Blend
        float alphaCutoff = 0.5f;
        uint32_t doubleSided = 0;
    };
    std::vector<MaterialEntry> materials;

    // Mesh data (pre-transformed vertices, indices)
    struct PrimitiveEntry {
        int32_t materialIndex = -1;
        std::vector<ModelVertex> vertices;
        std::vector<uint32_t> indices;
        BoundingBox localAABB;
    };
    struct MeshEntry {
        std::string name;
        std::vector<PrimitiveEntry> primitives;
    };
    std::vector<MeshEntry> meshes;

    BoundingBox modelAABB;
};

class ModelCache {
public:
    static std::string GetCachePath(const std::string& gltfPath);
    static bool IsCacheValid(const std::string& gltfPath);
    static bool LoadFromCache(RenderContext& ctx, const std::string& cachePath, Model& outModel);
    static bool WriteCache(const std::string& cachePath, const ModelCacheWriteData& data);
};

} // namespace showcase
