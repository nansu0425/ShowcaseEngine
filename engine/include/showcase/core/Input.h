#pragma once

#include <Windows.h>

#include <array>
#include <cstdint>

namespace showcase {

class Input {
public:
    void Update(HWND hwnd);

    bool IsKeyDown(int key) const { return m_keysCurrent[key]; }
    bool IsKeyPressed(int key) const { return m_keysCurrent[key] && !m_keysPrevious[key]; }
    bool IsKeyReleased(int key) const { return !m_keysCurrent[key] && m_keysPrevious[key]; }

    bool IsMouseButtonDown(int button) const { return m_mouseButtons[button]; }
    bool IsMouseButtonPressed(int button) const { return m_mouseButtons[button] && !m_prevMouseButtons[button]; }
    bool IsMouseButtonReleased(int button) const { return !m_mouseButtons[button] && m_prevMouseButtons[button]; }
    int GetMouseX() const { return m_mouseX; }
    int GetMouseY() const { return m_mouseY; }
    int GetMouseDeltaX() const { return m_mouseX - m_prevMouseX; }
    int GetMouseDeltaY() const { return m_mouseY - m_prevMouseY; }
    int GetMouseWheelDelta() const { return m_wheelDelta; }

    void SetMousePosition(int x, int y) {
        m_mouseX = x;
        m_mouseY = y;
        m_prevMouseX = x;
        m_prevMouseY = y;
    }
    void OnMouseWheel(int delta) { m_wheelDelta = delta; }
    void OnMouseMove(int x, int y);

private:
    std::array<bool, 256> m_keysCurrent = {};
    std::array<bool, 256> m_keysPrevious = {};
    std::array<bool, 3> m_mouseButtons = {}; // Left, Right, Middle
    std::array<bool, 3> m_prevMouseButtons = {};

    int m_mouseX = 0;
    int m_mouseY = 0;
    int m_prevMouseX = 0;
    int m_prevMouseY = 0;
    int m_wheelDelta = 0;
};

namespace Key {
constexpr int kEscape = 0x1B;  // VK_ESCAPE
constexpr int kShift = 0x10;   // VK_SHIFT
constexpr int kControl = 0x11; // VK_CONTROL
constexpr int kAlt = 0x12;     // VK_MENU
constexpr int kDelete = 0x2E;  // VK_DELETE
constexpr int kF5 = 0x74;      // VK_F5
constexpr int kF11 = 0x7A;     // VK_F11
} // namespace Key

} // namespace showcase
