#pragma once

#include <Windows.h>

#include <string>
#include <functional>
#include <cstdint>

namespace showcase {

struct WindowDesc {
    std::wstring title = L"ShowcaseEngine";
    uint32_t width = 1600;
    uint32_t height = 900;
};

class Window {
public:
    using ResizeCallback = std::function<void(uint32_t, uint32_t)>;

    Window() = default;
    ~Window();

    [[nodiscard]] bool Init(const WindowDesc& desc);
    void Shutdown();

    void SavePlacement() const;
    void RestorePlacement();

    [[nodiscard]] bool ProcessMessages();

    HWND GetHandle() const { return m_hwnd; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    float GetAspectRatio() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }
    bool IsMinimized() const { return m_minimized; }

    void SetResizeCallback(ResizeCallback callback) { m_resizeCallback = std::move(callback); }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

    HWND m_hwnd = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_minimized = false;
    ResizeCallback m_resizeCallback;
};

} // namespace showcase
