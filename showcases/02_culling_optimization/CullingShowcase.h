#pragma once

#include <showcase/demo/IShowcase.h>
#include <showcase/graphics/Buffer.h>
#include <showcase/graphics/Camera.h>
#include "FPSCameraController.h"
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace showcase {

class CullingShowcase : public IShowcase {
public:
    std::string GetName() const override { return "Culling Optimization"; }
    std::string GetDescription() const override {
        return "Demonstrates frustum, back-face, occlusion, and LOD/distance culling "
               "techniques on a mini-city scene.";
    }
    std::string GetCategory() const override { return "Optimization"; }

    void OnLoad(RenderContext& ctx) override;
    void OnUnload() override;
    void OnResize(uint32_t width, uint32_t height) override;
    void OnUpdate(float deltaTime, const Input& input) override;
    void OnRender(RenderContext& ctx) override;
    void OnImGui() override;

private:
    void CreateGridMesh(ID3D12Device* device, D3D12MA::Allocator* allocator);
    void CreateCubeMesh(ID3D12Device* device, D3D12MA::Allocator* allocator);

    // Pipeline
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    // Constant buffers
    Buffer m_perFrameCB;
    Buffer m_perObjectCB;

    // Geometry
    Buffer m_gridVB;
    Buffer m_gridIB;
    uint32_t m_gridIndexCount = 0;

    Buffer m_cubeVB;
    Buffer m_cubeIB;
    uint32_t m_cubeIndexCount = 0;

    // Camera
    Camera m_camera;
    FPSCameraController m_cameraController;
    float m_aspectRatio = 16.0f / 9.0f;

    static constexpr uint32_t kMaxObjects = 16;

    // Cube transforms
    struct CubeInstance {
        DirectX::SimpleMath::Matrix world;
    };
    std::vector<CubeInstance> m_cubes;
};

} // namespace showcase
