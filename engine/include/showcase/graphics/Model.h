#pragma once

#include <showcase/core/Math.h>
#include <showcase/graphics/Buffer.h>
#include <showcase/graphics/DescriptorHeap.h>
#include <showcase/graphics/Texture.h>

#include <memory>
#include <string>
#include <vector>

namespace showcase {

class RenderContext;

struct ModelVertex {
    Vector3 position;
    Vector3 normal;
    Vector2 texCoord;
};

struct Material {
    Vector4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    std::shared_ptr<Texture> baseColorTexture;
};

struct MeshPrimitive {
    Buffer vertexBuffer;
    Buffer indexBuffer;
    uint32_t indexCount = 0;
    std::shared_ptr<Material> material;
    BoundingBox localAABB;
};

struct Mesh {
    std::vector<MeshPrimitive> primitives;
    std::string name;
};

struct Model {
    std::vector<Mesh> meshes;
    BoundingBox localAABB;

    void Shutdown(RenderContext& ctx);
};

class ModelLoader {
public:
    [[nodiscard]] static bool LoadGLTF(RenderContext& ctx, const std::string& filepath, Model& outModel);
};

} // namespace showcase
