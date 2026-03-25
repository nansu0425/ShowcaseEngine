#include <showcase/editor/ViewportPanel.h>

#include <showcase/core/Input.h>
#include <showcase/core/Log.h>
#include <showcase/graphics/RenderContext.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace showcase {

// ── Init / Shutdown ──────────────────────────────────────────────

bool ViewportPanel::Init(RenderContext& ctx, uint32_t initialWidth, uint32_t initialHeight) {
    // Set up camera aspect ratio update on resize
    m_offscreenTarget.SetResizeCallback([this](uint32_t w, uint32_t h) {
        float aspect = h > 0 ? static_cast<float>(w) / h : 1.0f;
        m_camera.SetPerspective(m_camera.GetFovY(), aspect, m_camera.GetNearZ(), m_camera.GetFarZ());
    });

    return m_offscreenTarget.Init(ctx, initialWidth, initialHeight);
}

void ViewportPanel::Shutdown(RenderContext& ctx) {
    m_offscreenTarget.Shutdown(ctx);
}

// ── Rendering ────────────────────────────────────────────────────

void ViewportPanel::BeginRender(CommandList& cmdList) {
    m_offscreenTarget.BeginRender(cmdList);
}

void ViewportPanel::EndRender(CommandList& cmdList) {
    m_offscreenTarget.EndRender(cmdList);
}

// ── ImGui ─────────────────────────────────────────────────────────

void ViewportPanel::OnImGui(float fps, float deltaTime) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("Viewport")) {
        // Render toolbar (fixed bar at top, before scene image)
        if (m_toolbarCallback) {
            float pad = 6.0f;
            ImGui::SetCursorPos(ImVec2(pad, ImGui::GetCursorPosY() + pad));
            m_toolbarCallback();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + pad);
            ImGui::Separator();
        }

        ImVec2 size = ImGui::GetContentRegionAvail();
        uint32_t w = static_cast<uint32_t>(std::max(size.x, 1.0f));
        uint32_t h = static_cast<uint32_t>(std::max(size.y, 1.0f));

        // Detect resize
        m_offscreenTarget.RequestResize(w, h);

        // Display render target
        auto gpuHandle = m_offscreenTarget.GetRenderTarget().GetSRVHandle().gpu;
        ImGui::Image((ImTextureID)gpuHandle.ptr,
                     ImVec2(static_cast<float>(GetWidth()), static_cast<float>(GetHeight())));

        // Store image rect for external use (gizmo, hover detection)
        m_imageMin = ImGui::GetItemRectMin();
        m_imageMax = ImGui::GetItemRectMax();

        // FPS overlay drawn directly on the viewport's draw list
        if (m_showFPS) {
            ImVec2 imageMin = m_imageMin;
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 textPos(imageMin.x + 10, imageMin.y + 10);
            char fpsText[64];
            snprintf(fpsText, sizeof(fpsText), "FPS: %.1f\nFrame: %.2f ms", fps, deltaTime * 1000.0f);
            ImVec2 textSize = ImGui::CalcTextSize(fpsText);
            ImVec2 padding(6, 4);
            drawList->AddRectFilled(ImVec2(textPos.x - padding.x, textPos.y - padding.y),
                                    ImVec2(textPos.x + textSize.x + padding.x, textPos.y + textSize.y + padding.y),
                                    IM_COL32(0, 0, 0, 128), 4.0f);
            drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), fpsText);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// ── Camera ────────────────────────────────────────────────────────

void ViewportPanel::InitCamera(const Vector3& position, const Vector3& lookAt, float fovY, float nearZ, float farZ) {
    m_camera.SetPosition(position);
    m_camera.SetLookAt(lookAt);
    m_camera.SetPerspective(fovY, GetAspectRatio(), nearZ, farZ);
    m_camera.UpdateViewMatrix();
    m_firstCameraUpdate = true;
}

void ViewportPanel::InitCamera(const Vector3& position, float yaw, float pitch, float fovY, float nearZ, float farZ) {
    m_yaw = yaw;
    m_pitch = pitch;
    m_firstCameraUpdate = false;

    float cosP = std::cos(m_pitch);
    Vector3 forward(std::sin(m_yaw) * cosP, std::sin(m_pitch), std::cos(m_yaw) * cosP);
    forward.Normalize();

    m_camera.SetPosition(position);
    m_camera.SetLookAt(position + forward);
    m_camera.SetPerspective(fovY, GetAspectRatio(), nearZ, farZ);
    m_camera.UpdateViewMatrix();
}

void ViewportPanel::UpdateCamera(const Input& input, float deltaTime, bool playMode) {
    // Initialize yaw/pitch from current camera direction on first update
    if (m_firstCameraUpdate) {
        Vector3 fwd = m_camera.GetForward();
        m_yaw = std::atan2(fwd.x, fwd.z);
        m_pitch = std::asin(std::clamp(fwd.y, -1.0f, 1.0f));
        m_firstCameraUpdate = false;
    }

    // In play mode: always active. In edit mode: right-click to control.
    bool canControl = playMode || input.IsMouseButtonDown(1);

    // Mouse look
    if (canControl) {
        m_yaw += input.GetMouseDeltaX() * cameraLookSpeed;
        m_pitch -= input.GetMouseDeltaY() * cameraLookSpeed;
        m_pitch = std::clamp(m_pitch, -kPiOver2 * 0.98f, kPiOver2 * 0.98f);
    }

    // Compute direction vectors from yaw/pitch
    float cosP = std::cos(m_pitch);
    Vector3 forward(std::sin(m_yaw) * cosP, std::sin(m_pitch), std::cos(m_yaw) * cosP);
    forward.Normalize();
    Vector3 worldUp(0.0f, 1.0f, 0.0f);
    Vector3 right = worldUp.Cross(forward);
    right.Normalize();

    // WASD + QE movement
    Vector3 move(0.0f, 0.0f, 0.0f);
    if (canControl) {
        if (input.IsKeyDown('W')) {
            move += forward;
        }
        if (input.IsKeyDown('S')) {
            move -= forward;
        }
        if (input.IsKeyDown('D')) {
            move += right;
        }
        if (input.IsKeyDown('A')) {
            move -= right;
        }
        if (input.IsKeyDown('E')) {
            move += worldUp;
        }
        if (input.IsKeyDown('Q')) {
            move -= worldUp;
        }
    }

    if (move.LengthSquared() > 0.0f) {
        move.Normalize();
    }

    // Shift to boost speed
    float speed = cameraMoveSpeed;
    if (input.IsKeyDown(Key::kShift)) {
        speed *= 3.0f;
    }

    Vector3 pos = m_camera.GetPosition() + move * speed * deltaTime;

    m_camera.SetPosition(pos);
    m_camera.SetLookAt(pos + forward);
    m_camera.UpdateViewMatrix();
}

void ViewportPanel::SetResizeCallback(ResizeCallback callback) {
    // Chain with existing internal resize callback (camera update)
    m_offscreenTarget.SetResizeCallback([this, externalCb = std::move(callback)](uint32_t w, uint32_t h) {
        float aspect = h > 0 ? static_cast<float>(w) / h : 1.0f;
        m_camera.SetPerspective(m_camera.GetFovY(), aspect, m_camera.GetNearZ(), m_camera.GetFarZ());
        if (externalCb) {
            externalCb(w, h);
        }
    });
}

} // namespace showcase
