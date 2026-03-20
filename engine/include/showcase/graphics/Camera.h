#pragma once

#include <SimpleMath.h>
#include <DirectXCollision.h>

namespace showcase {

class Camera {
public:
    void SetPerspective(float fovY, float aspectRatio, float nearZ, float farZ);
    void SetPosition(const DirectX::SimpleMath::Vector3& position);
    void SetLookAt(const DirectX::SimpleMath::Vector3& target);
    void UpdateViewMatrix();

    const DirectX::SimpleMath::Matrix& GetViewMatrix() const { return m_viewMatrix; }
    const DirectX::SimpleMath::Matrix& GetProjectionMatrix() const { return m_projMatrix; }
    DirectX::SimpleMath::Matrix GetViewProjectionMatrix() const { return m_viewMatrix * m_projMatrix; }

    const DirectX::SimpleMath::Vector3& GetPosition() const { return m_position; }
    DirectX::SimpleMath::Vector3 GetForward() const { return m_forward; }
    DirectX::SimpleMath::Vector3 GetRight() const { return m_right; }
    DirectX::SimpleMath::Vector3 GetUp() const { return m_up; }

    float GetFovY() const { return m_fovY; }
    float GetNearZ() const { return m_nearZ; }
    float GetFarZ() const { return m_farZ; }
    float GetAspectRatio() const { return m_aspectRatio; }

    DirectX::BoundingFrustum GetBoundingFrustum() const;

private:
    DirectX::SimpleMath::Vector3 m_position = DirectX::SimpleMath::Vector3::Zero;
    DirectX::SimpleMath::Vector3 m_forward = DirectX::SimpleMath::Vector3::Forward;
    DirectX::SimpleMath::Vector3 m_up = DirectX::SimpleMath::Vector3::Up;
    DirectX::SimpleMath::Vector3 m_right = DirectX::SimpleMath::Vector3::Right;

    DirectX::SimpleMath::Matrix m_viewMatrix = DirectX::SimpleMath::Matrix::Identity;
    DirectX::SimpleMath::Matrix m_projMatrix = DirectX::SimpleMath::Matrix::Identity;

    float m_fovY = DirectX::XM_PIDIV4;
    float m_aspectRatio = 16.0f / 9.0f;
    float m_nearZ = 0.1f;
    float m_farZ = 1000.0f;

    bool m_viewDirty = true;
};

} // namespace showcase
