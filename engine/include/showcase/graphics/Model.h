#pragma once

#include <showcase/graphics/Buffer.h>
#include <showcase/graphics/Texture.h>
#include <showcase/graphics/DescriptorHeap.h>
#include <showcase/graphics/CommandQueue.h>
#include <SimpleMath.h>
#include <DirectXCollision.h>
#include <string>
#include <vector>

namespace showcase {

struct ModelVertex {
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 normal;
    DirectX::SimpleMath::Vector2 texCoord;
};

struct MeshPrimitive {
    Buffer vertexBuffer;
    Buffer indexBuffer;
    uint32_t indexCount = 0;
    int materialIndex = -1;
    DirectX::BoundingBox localAABB;
};

struct Mesh {
    std::vector<MeshPrimitive> primitives;
    std::string name;
};

struct Material {
    DirectX::SimpleMath::Vector4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    int baseColorTextureIndex = -1;
};

struct Model {
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    DirectX::BoundingBox localAABB;

    void Shutdown(DescriptorHeap& srvHeap);
};

class ModelLoader {
public:
    static bool LoadGLTF(const std::string& filepath,
                         ID3D12Device* device, D3D12MA::Allocator* allocator,
                         CommandQueue& cmdQueue, DescriptorHeap& srvHeap,
                         Model& outModel);
};

} // namespace showcase
