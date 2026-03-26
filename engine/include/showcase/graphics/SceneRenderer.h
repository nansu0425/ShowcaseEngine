#pragma once

#include <showcase/graphics/Buffer.h>
#include <showcase/graphics/Camera.h>
#include <showcase/graphics/DepthBuffer.h>
#include <showcase/graphics/Model.h>
#include <showcase/graphics/RenderTarget.h>
#include <showcase/graphics/Scene.h>

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace showcase {

class RenderContext;

struct PrimitiveHighlight {
    int objectId = -1;
    int meshIndex = -1;
    int primIndex = -1;
};

class SceneRenderer {
public:
    void Init(RenderContext& ctx);
    void Shutdown();
    void OnResize(uint32_t width, uint32_t height);
    void Render(RenderContext& ctx, Camera& camera, Scene& scene, int selectedObjectId,
                const PrimitiveHighlight& highlight = {});

    /// Renders object IDs to the pick buffer. Call after Render() each frame.
    void RenderObjectIds(RenderContext& ctx, Camera& camera, Scene& scene);

    /// Request a GPU pick at the given pixel coordinates (in ID RT space).
    void RequestPick(int pixelX, int pixelY);

    /// Returns true when a pick result is ready to read.
    bool IsPickComplete(RenderContext& ctx) const;

    /// Returns the picked object ID, or -1 if background. Only valid after IsPickComplete() returns true.
    int GetPickResult(RenderContext& ctx);

    // Procedural geometry helpers
    void CreateCubeModel(RenderContext& ctx, Model& outModel);
    void CreatePlaneModel(RenderContext& ctx, Model& outModel);
    void CreateSphereModel(RenderContext& ctx, Model& outModel);

    Texture& GetDefaultWhiteTexture() { return m_defaultWhiteTex; }

private:
    static constexpr uint32_t kMaxObjects = 256;

    // Pipeline
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12PipelineState> m_pipelineStateDoubleSided;

    // Selection outline
    ComPtr<ID3D12RootSignature> m_outlineRootSignature;
    ComPtr<ID3D12PipelineState> m_outlinePSO;

    // Constant buffers
    Buffer m_perFrameCB;
    Buffer m_perObjectCB;
    Buffer m_perMaterialCB;

    // Default white texture for untextured geometry
    Texture m_defaultWhiteTex;

    // Object ID picking
    ComPtr<ID3D12PipelineState> m_objectIdPSO;
    ComPtr<ID3D12PipelineState> m_objectIdPSODoubleSided;
    RenderTarget m_objectIdRT;
    DepthBuffer m_objectIdDepth;
    ComPtr<ID3D12Resource> m_pickReadbackBuffer;
    int m_pickX = 0;
    int m_pickY = 0;
    bool m_pickRequested = false;
    bool m_pickReady = false;
    uint64_t m_pickFenceValue = 0;
    int m_lastPickResult = -1;

    // Cached device pointers (non-owning)
    ID3D12Device* m_device = nullptr;
    D3D12MA::Allocator* m_allocator = nullptr;
    DescriptorHeap* m_srvHeap = nullptr;

    float m_aspectRatio = 16.0f / 9.0f;
};

} // namespace showcase
