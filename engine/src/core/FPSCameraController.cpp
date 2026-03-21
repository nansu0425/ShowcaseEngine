#include <showcase/core/FPSCameraController.h>
#include <showcase/graphics/Camera.h>
#include <showcase/core/Input.h>
#include <algorithm>
#include <cmath>

namespace showcase {

using namespace DirectX::SimpleMath;

void FPSCameraController::Update(Camera& camera, const Input& input, float deltaTime) {
    // Initialize yaw/pitch from current camera direction on first update
    if (m_firstUpdate) {
        Vector3 fwd = camera.GetForward();
        m_yaw = std::atan2(fwd.x, fwd.z);
        m_pitch = std::asin(std::clamp(fwd.y, -1.0f, 1.0f));
        m_firstUpdate = false;
    }

    // Mouse look (right-click drag to avoid ImGui conflict)
    if (input.IsMouseButtonDown(1)) {
        m_yaw += input.GetMouseDeltaX() * lookSpeed;
        m_pitch -= input.GetMouseDeltaY() * lookSpeed;
        m_pitch = std::clamp(m_pitch, -DirectX::XM_PIDIV2 * 0.98f, DirectX::XM_PIDIV2 * 0.98f);
    }

    // Compute direction vectors from yaw/pitch
    float cosP = std::cos(m_pitch);
    Vector3 forward(std::sin(m_yaw) * cosP, std::sin(m_pitch), std::cos(m_yaw) * cosP);
    forward.Normalize();
    Vector3 worldUp(0.0f, 1.0f, 0.0f);
    Vector3 right = worldUp.Cross(forward);
    right.Normalize();

    // WASD + QE movement (only while right-click held, to avoid conflict with gizmo shortcuts)
    Vector3 move(0.0f, 0.0f, 0.0f);
    if (input.IsMouseButtonDown(1)) {
        if (input.IsKeyDown('W')) move += forward;
        if (input.IsKeyDown('S')) move -= forward;
        if (input.IsKeyDown('D')) move += right;
        if (input.IsKeyDown('A')) move -= right;
        if (input.IsKeyDown('E')) move += worldUp;
        if (input.IsKeyDown('Q')) move -= worldUp;
    }

    if (move.LengthSquared() > 0.0f) {
        move.Normalize();
    }

    // Shift to boost speed
    float speed = moveSpeed;
    if (input.IsKeyDown(VK_SHIFT)) speed *= 3.0f;

    Vector3 pos = camera.GetPosition() + move * speed * deltaTime;

    camera.SetPosition(pos);
    camera.SetLookAt(pos + forward);
    camera.UpdateViewMatrix();
}

} // namespace showcase
