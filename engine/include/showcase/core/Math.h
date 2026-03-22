#pragma once

#include <SimpleMath.h>
#include <DirectXCollision.h>

namespace showcase {

// Core types
using Vector2    = DirectX::SimpleMath::Vector2;
using Vector3    = DirectX::SimpleMath::Vector3;
using Vector4    = DirectX::SimpleMath::Vector4;
using Matrix     = DirectX::SimpleMath::Matrix;
using Quaternion = DirectX::SimpleMath::Quaternion;
using Color      = DirectX::SimpleMath::Color;

// Collision types
using BoundingBox     = DirectX::BoundingBox;
using BoundingFrustum = DirectX::BoundingFrustum;

// Constants
inline constexpr float kPi      = DirectX::XM_PI;
inline constexpr float kPiOver2 = DirectX::XM_PIDIV2;
inline constexpr float kPiOver4 = DirectX::XM_PIDIV4;
inline constexpr float kTwoPi   = DirectX::XM_2PI;

// Angle conversion
inline float ToRadians(float degrees) { return DirectX::XMConvertToRadians(degrees); }
inline float ToDegrees(float radians) { return DirectX::XMConvertToDegrees(radians); }

// LH matrix helpers (engine coordinate system)
inline Matrix LookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up) {
    return DirectX::XMMatrixLookAtLH(eye, target, up);
}

inline Matrix PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ) {
    return DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
}

} // namespace showcase
