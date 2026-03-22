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

// Scale safety — prevent zero-scale causing singular matrices
inline constexpr float kMinScale = 1e-4f;

[[nodiscard]] inline Vector3 ClampScale(const Vector3& s) {
    auto clamp = [](float v) {
        if (v >= 0.0f) return v < kMinScale ? kMinScale : v;
        return v > -kMinScale ? -kMinScale : v;
    };
    return {clamp(s.x), clamp(s.y), clamp(s.z)};
}

// LH matrix helpers (engine coordinate system)
inline Matrix LookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up) {
    return DirectX::XMMatrixLookAtLH(eye, target, up);
}

inline Matrix PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ) {
    return DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
}

} // namespace showcase
