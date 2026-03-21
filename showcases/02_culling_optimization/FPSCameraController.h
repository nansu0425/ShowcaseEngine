#pragma once

namespace showcase {

class Camera;
class Input;

class FPSCameraController {
public:
    void Update(Camera& camera, const Input& input, float deltaTime);

    float moveSpeed = 10.0f;
    float lookSpeed = 0.003f;

private:
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    bool m_firstUpdate = true;
};

} // namespace showcase
