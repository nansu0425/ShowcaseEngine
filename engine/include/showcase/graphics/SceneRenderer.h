#pragma once

#include <showcase/graphics/Buffer.h>
#include <showcase/graphics/Camera.h>
#include <showcase/graphics/DepthBuffer.h>
#include <showcase/graphics/FrameResource.h>
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

struct ShadowDebugStats {
    Vector3 lightDirection;
    float frustumWidth = 0.0f;  // ortho projection width (meters)
    float frustumHeight = 0.0f; // ortho projection height (meters)
    float frustumNear = 0.0f;
    float frustumFar = 0.0f;
    float texelsPerMeter = 0.0f; // shadow map resolution / frustum width
    float shadowBias = 0.0f;
    float shadowDistance = 0.0f;
    uint32_t resolution = 0;
    bool active = false;
};

struct ShadowFrustumDebugData {
    Vector3 corners[8]; // world-space frustum corners (near 0-3, far 4-7)
    bool valid = false;
};

class SceneRenderer {
public:
    void Init(RenderContext& ctx);
    void Shutdown();
    void OnResize(uint32_t width, uint32_t height);

    /// Renders the shadow map from directional light. Call before BeginRender/Render.
    void RenderShadowPass(RenderContext& ctx, Camera& camera, Scene& scene);

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

    const ShadowDebugStats& GetShadowDebugStats() const { return m_shadowDebugStats; }
    const ShadowFrustumDebugData& GetShadowFrustumDebug() const { return m_shadowFrustumDebug; }

    /// Renders the shadow frustum as a depth-tested semi-transparent box.
    /// Call between Render() and viewport EndRender() while scene depth is populated.
    void RenderShadowFrustum(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                             D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint32_t width, uint32_t height);

    void RenderShadowPreview(RenderContext& ctx);
    D3D12_GPU_DESCRIPTOR_HANDLE GetShadowPreviewSRV() const;
    bool IsShadowPreviewReady() const { return m_shadowPreviewRendered; }

    /// Renders a semi-transparent shadow coverage overlay (red=shadowed, green=lit).
    /// Call between Render() and viewport EndRender() while scene depth is populated.
    void RenderShadowOverlay(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                             DepthBuffer& sceneDepthBuffer, uint32_t width, uint32_t height);

private:
    static constexpr uint32_t kMaxObjects = 256;

    // Pipeline
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12PipelineState> m_pipelineStateDoubleSided;

    // Selection outline
    ComPtr<ID3D12RootSignature> m_outlineRootSignature;
    ComPtr<ID3D12PipelineState> m_outlinePSO;

    // Shadow frustum debug visualization
    ComPtr<ID3D12RootSignature> m_frustumDebugRootSig;
    ComPtr<ID3D12PipelineState> m_frustumDebugTriPSO;
    ComPtr<ID3D12PipelineState> m_frustumDebugLinePSO;

    // Constant buffers (per-frame to avoid CPU/GPU race on upload heap)
    Buffer m_perFrameCB[FrameResource::kNumFrames];
    Buffer m_perObjectCB[FrameResource::kNumFrames];
    Buffer m_perMaterialCB[FrameResource::kNumFrames];

    // Default white texture for untextured geometry
    Texture m_defaultWhiteTex;

    // Shadow mapping
    DepthBuffer m_shadowMap;
    Buffer m_shadowPerFrameCB[FrameResource::kNumFrames];
    ComPtr<ID3D12PipelineState> m_shadowPSO;
    ComPtr<ID3D12PipelineState> m_shadowPSODoubleSided;
    static constexpr uint32_t kShadowMapResolution = 2048;
    bool m_shadowMapReady = false; // tracks initial resource barrier state
    Matrix m_cachedLightVP = {};   // computed by RenderShadowPass, used by Render
    bool m_hasShadow = false;      // set by RenderShadowPass for current frame
    float m_cachedShadowBias = 0.001f;
    ShadowDebugStats m_shadowDebugStats;
    ShadowFrustumDebugData m_shadowFrustumDebug;

    // Shadow map preview (depth-to-grayscale conversion)
    RenderTarget m_shadowPreviewRT;
    ComPtr<ID3D12RootSignature> m_shadowPreviewRootSig;
    ComPtr<ID3D12PipelineState> m_shadowPreviewPSO;

    // Shadow coverage overlay
    ComPtr<ID3D12RootSignature> m_shadowOverlayRootSig;
    ComPtr<ID3D12PipelineState> m_shadowOverlayPSO;
    static constexpr uint32_t kShadowPreviewSize = 512;
    bool m_shadowPreviewRendered = false;

    void RenderShadowMap(RenderContext& ctx, Scene& scene, const Matrix& lightViewProj);

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
    bool m_objectIdNeedsInit = false;

    // Cached device pointers (non-owning)
    ID3D12Device* m_device = nullptr;
    D3D12MA::Allocator* m_allocator = nullptr;
    DescriptorHeap* m_srvHeap = nullptr;

    float m_aspectRatio = 16.0f / 9.0f;
};

} // namespace showcase
