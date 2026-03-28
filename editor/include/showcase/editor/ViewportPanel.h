#pragma once

#include <showcase/graphics/Camera.h>
#include <showcase/graphics/OffscreenTarget.h>

#include <imgui.h>

#include <cstdint>
#include <functional>

namespace showcase {

enum class ViewMode : int;

class Input;
class RenderContext;

using ToolbarCallback = std::function<void()>;

class ViewportPanel {
public:
    [[nodiscard]] bool Init(RenderContext& ctx, uint32_t initialWidth, uint32_t initialHeight);
    void Shutdown(RenderContext& ctx);

    void BeginRender(CommandList& cmdList);
    void EndRender(CommandList& cmdList);
    void OnImGui(float fps, float deltaTime);

    void InitCamera(const Vector3& position, const Vector3& lookAt, float fovY, float nearZ, float farZ);
    void UpdateCamera(const Input& input, float deltaTime, bool playMode = false);

    Camera& GetCamera() { return m_camera; }
    const Camera& GetCamera() const { return m_camera; }

    OffscreenTarget& GetOffscreenTarget() { return m_offscreenTarget; }

    void SetResizeCallback(ResizeCallback callback);
    void SetToolbarCallback(ToolbarCallback callback) { m_toolbarCallback = std::move(callback); }

    void ToggleShowFPS() { m_showFPS = !m_showFPS; }
    bool GetShowFPS() const { return m_showFPS; }
    void SetShowFPS(bool show) { m_showFPS = show; }

    void ToggleShowShadowFrustum() { m_showShadowFrustum = !m_showShadowFrustum; }
    bool GetShowShadowFrustum() const { return m_showShadowFrustum; }
    void SetShowShadowFrustum(bool show) { m_showShadowFrustum = show; }

    void ToggleShowShadowOverlay() { m_showShadowOverlay = !m_showShadowOverlay; }
    bool GetShowShadowOverlay() const { return m_showShadowOverlay; }
    void SetShowShadowOverlay(bool show) { m_showShadowOverlay = show; }

    void ToggleShowPointShadowOverlay() { m_showPointShadowOverlay = !m_showPointShadowOverlay; }
    bool GetShowPointShadowOverlay() const { return m_showPointShadowOverlay; }
    void SetShowPointShadowOverlay(bool show) { m_showPointShadowOverlay = show; }

    void ToggleShowCubemapFaceOverlay() { m_showCubemapFaceOverlay = !m_showCubemapFaceOverlay; }
    bool GetShowCubemapFaceOverlay() const { return m_showCubemapFaceOverlay; }
    void SetShowCubemapFaceOverlay(bool show) { m_showCubemapFaceOverlay = show; }

    void ToggleShowDepthHeatmapOverlay() { m_showDepthHeatmapOverlay = !m_showDepthHeatmapOverlay; }
    bool GetShowDepthHeatmapOverlay() const { return m_showDepthHeatmapOverlay; }
    void SetShowDepthHeatmapOverlay(bool show) { m_showDepthHeatmapOverlay = show; }

    ViewMode GetViewMode() const { return m_viewMode; }
    void SetViewMode(ViewMode mode) { m_viewMode = mode; }

    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }
    void SetYaw(float yaw) { m_yaw = yaw; }
    void SetPitch(float pitch) { m_pitch = pitch; }

    void InitCamera(const Vector3& position, float yaw, float pitch, float fovY, float nearZ, float farZ);

    ImVec2 GetImageMin() const { return m_imageMin; }
    ImVec2 GetImageMax() const { return m_imageMax; }

    uint32_t GetWidth() const { return m_offscreenTarget.GetWidth(); }
    uint32_t GetHeight() const { return m_offscreenTarget.GetHeight(); }
    float GetAspectRatio() const { return m_offscreenTarget.GetAspectRatio(); }

    float cameraMoveSpeed = 10.0f;
    float cameraLookSpeed = 0.003f;

private:
    OffscreenTarget m_offscreenTarget;

    bool m_showFPS = true;
    bool m_showShadowFrustum = false;
    bool m_showShadowOverlay = false;
    bool m_showPointShadowOverlay = false;
    bool m_showCubemapFaceOverlay = false;
    bool m_showDepthHeatmapOverlay = false;
    ViewMode m_viewMode = static_cast<ViewMode>(0); // ViewMode::Lit
    ToolbarCallback m_toolbarCallback;

    ImVec2 m_imageMin = {};
    ImVec2 m_imageMax = {};

    // Camera
    Camera m_camera;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    bool m_firstCameraUpdate = true;
};

} // namespace showcase
