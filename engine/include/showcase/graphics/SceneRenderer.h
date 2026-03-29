#pragma once

#include <showcase/graphics/Buffer.h>
#include <showcase/graphics/Camera.h>
#include <showcase/graphics/CubemapDepthBuffer.h>
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

enum class ViewMode : int {
    Lit = 0,
    Unlit = 1,
};

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

struct SpotShadowFrustumDebugData {
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

    /// Renders cubemap shadow maps for point lights. Call after RenderShadowPass, before Render.
    void RenderPointShadowPass(RenderContext& ctx, Scene& scene);
    void RenderSpotShadowPass(RenderContext& ctx, Scene& scene);

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
    const SpotShadowFrustumDebugData& GetSpotShadowFrustumDebug(int index) const {
        static const SpotShadowFrustumDebugData kInvalid{};
        if (index < 0 || index >= kMaxSpotShadowLights)
            return kInvalid;
        return m_spotShadowFrustumDebug[index];
    }

    /// Returns the shadow slot index (0..5) for a point light with the given object ID,
    /// or -1 if the light has no shadow slot assigned this frame.
    int GetPointShadowIndex(int objectId) const;
    static constexpr uint32_t GetPointShadowResolution() { return kPointShadowMapResolution; }
    static constexpr float GetPointShadowNearZ() { return kPointShadowNearZ; }

    /// Returns the shadow slot index (0..3) for a spot light with the given object ID,
    /// or -1 if the light has no shadow slot assigned this frame.
    int GetSpotShadowIndex(int objectId) const;
    /// Returns the index (0..7) for a spot light with the given object ID in the ALL-spot-lights array,
    /// or -1 if the object is not a spot light this frame.
    int GetSpotLightIndex(int objectId) const;
    static constexpr uint32_t GetSpotShadowResolution() { return kSpotShadowMapResolution; }
    static constexpr float GetSpotShadowNearZ() { return kSpotShadowNearZ; }

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

    /// Renders a semi-transparent point shadow coverage overlay (red=shadowed, green=lit, blue=outside range).
    /// Call between Render() and viewport EndRender() while scene depth is populated.
    void RenderPointShadowOverlay(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                                  DepthBuffer& sceneDepthBuffer, uint32_t width, uint32_t height, int shadowIndex);

    /// Renders cubemap face ID overlay (6 colors per face direction).
    /// Call between Render() and viewport EndRender() while scene depth is populated.
    void RenderCubemapFaceOverlay(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                                  DepthBuffer& sceneDepthBuffer, uint32_t width, uint32_t height, int shadowIndex);

    /// Renders depth precision heatmap overlay (green=close/good, red=far/poor, magenta=danger zone).
    /// Call between Render() and viewport EndRender() while scene depth is populated.
    void RenderDepthHeatmapOverlay(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                                   DepthBuffer& sceneDepthBuffer, uint32_t width, uint32_t height, int shadowIndex);

    /// Renders a semi-transparent spot shadow coverage overlay (red=shadowed, green=lit, blue=outside cone/range).
    /// Call between Render() and viewport EndRender() while scene depth is populated.
    void RenderSpotShadowOverlay(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                                 DepthBuffer& sceneDepthBuffer, uint32_t width, uint32_t height, int shadowIndex);

    /// Renders a spot light attenuation heatmap overlay (green=high, yellow=mid, red=low, transparent=outside).
    /// Call between Render() and viewport EndRender() while scene depth is populated.
    void RenderSpotAttenuationOverlay(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                                      DepthBuffer& sceneDepthBuffer, uint32_t width, uint32_t height, int lightIndex);

    /// Renders a spot shadow frustum as a depth-tested semi-transparent pyramid (wireframe + fill).
    /// Call between Render() and viewport EndRender() while scene depth is populated.
    void RenderSpotShadowFrustum(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                                 D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint32_t width, uint32_t height, int shadowIndex);

    void RenderCubemapPreview(RenderContext& ctx, int shadowIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE GetCubemapPreviewFaceSRV(uint32_t face) const;
    bool IsCubemapPreviewReady() const { return m_cubemapPreviewRendered; }

    void RenderSpotShadowPreview(RenderContext& ctx, int shadowIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE GetSpotShadowPreviewSRV() const;
    bool IsSpotShadowPreviewReady() const { return m_spotShadowPreviewRendered; }

    void SetViewMode(ViewMode mode) { m_viewMode = mode; }
    ViewMode GetViewMode() const { return m_viewMode; }

    /// Sets the point light gizmo data for GPU-based depth-tested wireframe rendering.
    void SetPointLightGizmo(const Vector3& center, float range);
    void ClearPointLightGizmo();

    /// Renders point light range wireframe (3 orthogonal rings) with depth testing.
    /// Call between Render() and viewport EndRender() while scene depth is populated.
    void RenderPointLightGizmo(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                               D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint32_t width, uint32_t height);

    struct SpotLightGizmoDesc {
        Vector3 position;
        Vector3 forward;
        Vector3 right;
        Vector3 up;
        float range;
        float outerAngle;
        float innerAngle;
    };

    /// Sets the spot light gizmo data for GPU-based depth-tested wireframe rendering.
    void SetSpotLightGizmo(const SpotLightGizmoDesc& desc);
    void ClearSpotLightGizmo();

    /// Renders spot light cone wireframe (outer + inner cones) with depth testing.
    void RenderSpotLightGizmo(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                              D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint32_t width, uint32_t height);

    static constexpr int kMaxSpotShadowLights = 8;

private:
    static constexpr uint32_t kMaxObjects = 256;
    static constexpr int kMaxPointLights = 6;

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

    // Point shadow coverage overlay
    ComPtr<ID3D12RootSignature> m_pointShadowOverlayRootSig;
    ComPtr<ID3D12PipelineState> m_pointShadowOverlayPSO;

    // Cubemap face ID overlay
    ComPtr<ID3D12RootSignature> m_cubemapFaceOverlayRootSig;
    ComPtr<ID3D12PipelineState> m_cubemapFaceOverlayPSO;

    // Depth precision heatmap overlay (shares root sig with cubemap face overlay)
    ComPtr<ID3D12PipelineState> m_depthHeatmapOverlayPSO;

    // Spot shadow coverage overlay
    ComPtr<ID3D12RootSignature> m_spotShadowOverlayRootSig;
    ComPtr<ID3D12PipelineState> m_spotShadowOverlayPSO;

    // Spot attenuation overlay (heatmap: distance * angular falloff)
    ComPtr<ID3D12RootSignature> m_spotAttenuationOverlayRootSig;
    ComPtr<ID3D12PipelineState> m_spotAttenuationOverlayPSO;

    // Cubemap face preview (point shadow depth-to-grayscale)
    RenderTarget m_cubemapPreviewRT[6];
    ComPtr<ID3D12RootSignature> m_cubemapPreviewRootSig;
    ComPtr<ID3D12PipelineState> m_cubemapPreviewPSO;
    static constexpr uint32_t kCubemapPreviewFaceSize = 128;
    bool m_cubemapPreviewRendered = false;

    // Spot shadow preview (perspective depth-to-grayscale)
    RenderTarget m_spotShadowPreviewRT;
    ComPtr<ID3D12RootSignature> m_spotShadowPreviewRootSig;
    ComPtr<ID3D12PipelineState> m_spotShadowPreviewPSO;
    bool m_spotShadowPreviewRendered = false;

    // Point light gizmo (depth-tested wireframe rings)
    struct PointLightGizmoData {
        Vector3 center;
        float range = 0.0f;
        bool valid = false;
    };
    PointLightGizmoData m_pointLightGizmo;
    ComPtr<ID3D12RootSignature> m_lightGizmoRootSig;
    ComPtr<ID3D12PipelineState> m_lightGizmoLinePSO;

    // Spot light gizmo (depth-tested wireframe cone)
    struct SpotLightGizmoData {
        Vector3 position;
        Vector3 forward;
        Vector3 right;
        Vector3 up;
        float range = 0.0f;
        float outerAngle = 0.0f;
        float innerAngle = 0.0f;
        bool valid = false;
    };
    SpotLightGizmoData m_spotLightGizmo;
    ComPtr<ID3D12RootSignature> m_spotLightGizmoRootSig;
    ComPtr<ID3D12PipelineState> m_spotLightGizmoLinePSO;
    static constexpr uint32_t kShadowPreviewSize = 512;
    bool m_shadowPreviewRendered = false;

    void RenderShadowMap(RenderContext& ctx, Scene& scene, const Matrix& lightViewProj);
    void RenderDepthOnlyObjects(RenderContext& ctx, Scene& scene, const Matrix& viewProj,
                                D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint32_t resolution, uint32_t shadowCBSlot);

    // Point light shadow mapping
    static constexpr uint32_t kPointShadowMapResolution = 512;
    static constexpr float kPointShadowNearZ = 0.1f;
    static constexpr uint32_t kSpotShadowMapResolution = 1024;
    static constexpr float kSpotShadowNearZ = 0.1f;
    static constexpr uint32_t kMaxShadowCBSlots =
        1 + kMaxPointLights * 6 + kMaxSpotShadowLights; // 1 dir + 36 point faces + 8 spot
    CubemapDepthBuffer m_pointShadowMaps[kMaxPointLights];
    bool m_pointShadowMapsReady = false;
    int m_numPointShadowLights = 0;
    Vector3 m_pointShadowPositions[kMaxPointLights];
    float m_pointShadowRanges[kMaxPointLights];
    float m_pointShadowBiases[kMaxPointLights];

    // Cached shadow index per point light (populated in Render(), queried by editor)
    struct CachedPointShadowEntry {
        int objectId = -1;
        int shadowIndex = -1;
    };
    CachedPointShadowEntry m_cachedPointShadowEntries[kMaxPointLights];
    int m_numCachedPointShadowEntries = 0;

    // Spot light shadow mapping
    DepthBuffer m_spotShadowMaps[kMaxSpotShadowLights];
    bool m_spotShadowMapsReady = false;
    int m_numSpotShadowLights = 0;
    Vector3 m_spotShadowPositions[kMaxSpotShadowLights];
    float m_spotShadowRanges[kMaxSpotShadowLights];
    float m_spotShadowBiases[kMaxSpotShadowLights];
    Matrix m_spotShadowVPs[kMaxSpotShadowLights];
    Vector3 m_spotShadowDirections[kMaxSpotShadowLights];
    float m_spotShadowOuterCos[kMaxSpotShadowLights];

    struct CachedSpotShadowEntry {
        int objectId = -1;
        int shadowIndex = -1;
    };
    CachedSpotShadowEntry m_cachedSpotShadowEntries[kMaxSpotShadowLights];
    int m_numCachedSpotShadowEntries = 0;

    SpotShadowFrustumDebugData m_spotShadowFrustumDebug[kMaxSpotShadowLights];

    // Spot light data (ALL spot lights — shadow and non-shadow — populated in Render())
    static constexpr int kMaxSpotLights = 8;
    Vector3 m_spotLightPositions[kMaxSpotLights];
    float m_spotLightRanges[kMaxSpotLights];
    Vector3 m_spotLightDirections[kMaxSpotLights];
    float m_spotLightOuterCos[kMaxSpotLights];
    float m_spotLightInnerCos[kMaxSpotLights];
    int m_numSpotLightsTracked = 0;

    struct CachedSpotLightEntry {
        int objectId = -1;
        int lightIndex = -1;
    };
    CachedSpotLightEntry m_cachedSpotLightEntries[kMaxSpotLights];
    int m_numCachedSpotLightEntries = 0;

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
    ViewMode m_viewMode = ViewMode::Lit;
};

} // namespace showcase
