#include <showcase/core/Input.h>

namespace showcase {

void Input::Update(HWND hwnd) {
    m_keysPrevious = m_keysCurrent;

    for (int i = 0; i < 256; i++) {
        m_keysCurrent[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
    }

    m_mouseButtons[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    m_mouseButtons[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    m_mouseButtons[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

    m_prevMouseX = m_mouseX;
    m_prevMouseY = m_mouseY;
    POINT pt;
    if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt)) {
        m_mouseX = pt.x;
        m_mouseY = pt.y;
    }

    m_wheelDelta = 0;
}

void Input::OnMouseMove(int x, int y) {
    m_prevMouseX = m_mouseX;
    m_prevMouseY = m_mouseY;
    m_mouseX = x;
    m_mouseY = y;
}

} // namespace showcase
