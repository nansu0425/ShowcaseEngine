#pragma once

#include <showcase/core/Math.h>

namespace showcase {

class Camera {
public:
    void SetPerspective(float fovY, float aspectRatio, float nearZ, float farZ);
    void SetPosition(const Vector3& position);
    void SetLookAt(const Vector3& target);
    void UpdateViewMatrix();

    const Matrix& GetViewMatrix() const { return m_viewMatrix; }
    const Matrix& GetProjectionMatrix() const { return m_projMatrix; }
    Matrix GetViewProjectionMatrix() const { return m_viewMatrix * m_projMatrix; }

    const Vector3& GetPosition() const { return m_position; }
    Vector3 GetForward() const { return m_forward; }
    Vector3 GetRight() const { return m_right; }
    Vector3 GetUp() const { return m_up; }

    float GetFovY() const { return m_fovY; }
    float GetNearZ() const { return m_nearZ; }
    float GetFarZ() const { return m_farZ; }
    float GetAspectRatio() const { return m_aspectRatio; }

    BoundingFrustum GetBoundingFrustum() const;

private:
    Vector3 m_position = {0.0f, 0.0f, 0.0f};
    Vector3 m_forward = {0.0f, 0.0f, 1.0f};
    Vector3 m_up = {0.0f, 1.0f, 0.0f};
    Vector3 m_right = {1.0f, 0.0f, 0.0f};

    Matrix m_viewMatrix = {};
    Matrix m_projMatrix = {};

    float m_fovY = PiOver4;
    float m_aspectRatio = 16.0f / 9.0f;
    float m_nearZ = 0.1f;
    float m_farZ = 1000.0f;

    bool m_viewDirty = true;
};

} // namespace showcase
