#pragma once

#include <showcase/graphics/Buffer.h>
#include <showcase/graphics/Camera.h>
#include <showcase/graphics/Model.h>
#include <showcase/graphics/Scene.h>
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace showcase {

class RenderContext;

class SceneRenderer {
public:
    void Init(RenderContext& ctx);
    void Shutdown();
    void OnResize(uint32_t width, uint32_t height);
    void Render(RenderContext& ctx, Camera& camera, Scene& scene, int selectedObjectId);

    /// Casts a ray from screen coordinates and returns the closest hit object ID, or -1 if none.
    int PickObject(int mouseX, int mouseY, const Camera& camera, const Scene& scene,
                   float vpMinX, float vpMinY, float vpMaxX, float vpMaxY) const;

    // Procedural geometry helpers
    void CreateGridModel(RenderContext& ctx, Model& outModel);
    void CreateCubeModel(RenderContext& ctx, Model& outModel);

    Texture& GetDefaultWhiteTexture() { return m_defaultWhiteTex; }

private:
    static constexpr uint32_t kMaxObjects = 64;

    // Pipeline
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    // Constant buffers
    Buffer m_perFrameCB;
    Buffer m_perObjectCB;
    Buffer m_perMaterialCB;

    // Default white texture for untextured geometry
    Texture m_defaultWhiteTex;

    // SRV heap pointer for cleanup
    DescriptorHeap* m_srvHeap = nullptr;

    float m_aspectRatio = 16.0f / 9.0f;
};

} // namespace showcase
