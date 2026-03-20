#include <showcase/graphics/Camera.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace showcase {

void Camera::SetPerspective(float fovY, float aspectRatio, float nearZ, float farZ)
{
    m_fovY = fovY;
    m_aspectRatio = aspectRatio;
    m_nearZ = nearZ;
    m_farZ = farZ;
    m_projMatrix = Matrix::CreatePerspectiveFieldOfView(fovY, aspectRatio, nearZ, farZ);
}

void Camera::SetPosition(const Vector3& position)
{
    m_position = position;
    m_viewDirty = true;
}

void Camera::SetLookAt(const Vector3& target)
{
    Vector3 direction = target - m_position;
    direction.Normalize();

    m_forward = direction;
    m_right = Vector3::Up.Cross(m_forward);
    m_right.Normalize();
    m_up = m_forward.Cross(m_right);
    m_up.Normalize();

    m_viewDirty = true;
}

void Camera::UpdateViewMatrix()
{
    if (!m_viewDirty)
        return;

    m_viewMatrix = Matrix::CreateLookAt(m_position, m_position + m_forward, m_up);
    m_viewDirty = false;
}

BoundingFrustum Camera::GetBoundingFrustum() const
{
    BoundingFrustum frustum(m_projMatrix);

    Matrix inverseView = m_viewMatrix.Invert();
    BoundingFrustum worldFrustum;
    frustum.Transform(worldFrustum, inverseView);

    return worldFrustum;
}

} // namespace showcase
