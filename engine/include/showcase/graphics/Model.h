#pragma once

#include <showcase/graphics/Buffer.h>
#include <showcase/graphics/Texture.h>
#include <showcase/graphics/DescriptorHeap.h>
#include <showcase/core/Math.h>

#include <string>
#include <vector>

namespace showcase {

class RenderContext;

struct ModelVertex {
    Vector3 position;
    Vector3 normal;
    Vector2 texCoord;
};

struct MeshPrimitive {
    Buffer vertexBuffer;
    Buffer indexBuffer;
    uint32_t indexCount = 0;
    int materialIndex = -1;
    BoundingBox localAABB;
};

struct Mesh {
    std::vector<MeshPrimitive> primitives;
    std::string name;
};

struct Material {
    Vector4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    int baseColorTextureIndex = -1;
};

struct Model {
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    BoundingBox localAABB;

    void Shutdown(RenderContext& ctx);
};

class ModelLoader {
public:
    [[nodiscard]] static bool LoadGLTF(RenderContext& ctx,
                                       const std::string& filepath,
                                       Model& outModel);
};

} // namespace showcase
