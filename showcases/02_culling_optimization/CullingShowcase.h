#pragma once

#include <showcase/demo/IShowcase.h>
#include <showcase/graphics/Buffer.h>
#include <showcase/graphics/Camera.h>
#include <showcase/graphics/Model.h>
#include <showcase/graphics/Scene.h>
#include "FPSCameraController.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <imgui.h>
#include <ImGuizmo.h>

using Microsoft::WRL::ComPtr;

namespace showcase {

class Viewport;

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
    void OnViewportToolbar() override;

private:
    void CreateGridModel(ID3D12Device* device, D3D12MA::Allocator* allocator);
    void CreateCubeModel(ID3D12Device* device, D3D12MA::Allocator* allocator);
    int PickObject(int mouseX, int mouseY) const;

    // Pipeline
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    // Constant buffers
    Buffer m_perFrameCB;
    Buffer m_perObjectCB;
    Buffer m_perMaterialCB;

    // Procedural models (own geometry buffers)
    Model m_gridModel;
    Model m_cubeModel;

    // Camera
    Camera m_camera;
    FPSCameraController m_cameraController;
    float m_aspectRatio = 16.0f / 9.0f;

    static constexpr uint32_t kMaxObjects = 64;

    // Scene
    Scene m_scene;
    int m_selectedObjectId = -1;

    // Viewport picking state (1-frame delayed from OnImGui)
    ImVec2 m_viewportMin = {};
    ImVec2 m_viewportMax = {};
    bool m_viewportHovered = false;

    // glTF model
    Model m_testModel;
    bool m_modelLoaded = false;
    DescriptorHeap* m_srvHeap = nullptr;

    // Default white texture for untextured geometry
    Texture m_defaultWhiteTex;

    // Viewport reference (for image rect)
    Viewport* m_viewportPtr = nullptr;

    // Gizmo
    ImGuizmo::OPERATION m_gizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_gizmoMode = ImGuizmo::LOCAL;
    bool m_useSnap = false;
    float m_snapTranslate = 1.0f;
    float m_snapRotate = 15.0f;
    float m_snapScale = 0.1f;
};

} // namespace showcase
