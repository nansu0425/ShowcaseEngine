#include <showcase/core/Window.h>

#include <showcase/core/JsonDocument.h>
#include <showcase/core/Log.h>
#include <showcase/core/Profiler.h>

// Forward declaration for ImGui Win32 handler
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

std::string GetConfigPath() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string exePath(path);
    return exePath.substr(0, exePath.find_last_of("\\/") + 1) + "engine_config.json";
}

} // anonymous namespace

namespace showcase {

Window::~Window() {
    Shutdown();
}

// ── Init / Shutdown ───────────────────────────────────────────────
bool Window::Init(const WindowDesc& desc) {
    SE_ZONE_SCOPED;
    m_width = desc.width;
    m_height = desc.height;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ShowcaseEngineWindow";

    if (!RegisterClassExW(&wc)) {
        SE_LOG_ERROR("Failed to register window class");
        return false;
    }

    RECT windowRect = {0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height)};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    m_hwnd = CreateWindowExW(0, wc.lpszClassName, desc.title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr, nullptr,
                             wc.hInstance, this);

    if (!m_hwnd) {
        SE_LOG_ERROR("Failed to create window");
        return false;
    }

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    SE_LOG_INFO("Window created: {}x{}", m_width, m_height);
    return true;
}

void Window::Shutdown() {
    SE_ZONE_SCOPED;
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void Window::SetTitle(const char* title) {
    if (m_hwnd) {
        SetWindowTextA(m_hwnd, title);
    }
}

// ── Fullscreen toggle ─────────────────────────────────────────────
void Window::ToggleFullscreen() {
    if (!m_hwnd)
        return;

    if (!m_fullscreen) {
        m_savedStyle = GetWindowLongPtr(m_hwnd, GWL_STYLE);
        m_savedPlacement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(m_hwnd, &m_savedPlacement);

        SetWindowLongPtr(m_hwnd, GWL_STYLE, m_savedStyle & ~(WS_CAPTION | WS_THICKFRAME));

        MONITORINFO mi = {sizeof(mi)};
        if (!GetMonitorInfo(MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLongPtr(m_hwnd, GWL_STYLE, m_savedStyle);
            SE_LOG_ERROR("Failed to get monitor info for fullscreen");
            return;
        }
        SetWindowPos(m_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED);

        m_fullscreen = true;
        SE_LOG_INFO("Entered fullscreen");
    } else {
        SetWindowLongPtr(m_hwnd, GWL_STYLE, m_savedStyle);
        SetWindowPlacement(m_hwnd, &m_savedPlacement);
        SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

        m_fullscreen = false;
        SE_LOG_INFO("Exited fullscreen");
    }
}

// ── State persistence ─────────────────────────────────────────────
void Window::SavePlacement() const {
    SE_ZONE_SCOPED_C(profile::kColorAssetIO);
    if (!m_hwnd)
        return;

    // If fullscreen, save the windowed placement we stored before entering fullscreen
    if (m_fullscreen) {
        JsonDocument doc;
        doc["showCmd"].Set(static_cast<int>(m_savedPlacement.showCmd));
        JsonNode np = doc["normalPosition"];
        np["left"].Set(static_cast<long>(m_savedPlacement.rcNormalPosition.left));
        np["top"].Set(static_cast<long>(m_savedPlacement.rcNormalPosition.top));
        np["right"].Set(static_cast<long>(m_savedPlacement.rcNormalPosition.right));
        np["bottom"].Set(static_cast<long>(m_savedPlacement.rcNormalPosition.bottom));
        doc["fullscreen"].Set(true);

        if (doc.SaveToFile(GetConfigPath())) {
            SE_LOG_INFO("Window placement saved (from pre-fullscreen state)");
        }
        return;
    }

    WINDOWPLACEMENT wp = {};
    wp.length = sizeof(WINDOWPLACEMENT);
    if (!GetWindowPlacement(m_hwnd, &wp))
        return;

    JsonDocument doc;
    doc["showCmd"].Set(static_cast<int>(wp.showCmd));
    auto np = doc["normalPosition"];
    np["left"].Set(static_cast<long>(wp.rcNormalPosition.left));
    np["top"].Set(static_cast<long>(wp.rcNormalPosition.top));
    np["right"].Set(static_cast<long>(wp.rcNormalPosition.right));
    np["bottom"].Set(static_cast<long>(wp.rcNormalPosition.bottom));
    doc["fullscreen"].Set(false);

    if (doc.SaveToFile(GetConfigPath())) {
        SE_LOG_INFO("Window placement saved");
    }
}

void Window::RestorePlacement() {
    SE_ZONE_SCOPED_C(profile::kColorAssetIO);
    if (!m_hwnd)
        return;

    JsonDocument doc;
    if (!doc.LoadFromFile(GetConfigPath())) {
        return;
    }

    WINDOWPLACEMENT wp = {};
    wp.length = sizeof(WINDOWPLACEMENT);

    auto np = doc["normalPosition"];
    wp.rcNormalPosition.left = np["left"].GetLong();
    wp.rcNormalPosition.top = np["top"].GetLong();
    wp.rcNormalPosition.right = np["right"].GetLong();
    wp.rcNormalPosition.bottom = np["bottom"].GetLong();
    wp.showCmd = static_cast<UINT>(doc["showCmd"].GetInt());

    SetWindowPlacement(m_hwnd, &wp);
    SE_LOG_INFO("Window placement restored");

    if (doc["fullscreen"].GetBool()) {
        ToggleFullscreen();
    }
}

// ── Message processing ────────────────────────────────────────────
bool Window::ProcessMessages() {
    SE_ZONE_SCOPED_C(profile::kColorUpdate);
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

// ── Win32 message handling ────────────────────────────────────────
LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    Window* window = nullptr;

    if (msg == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lparam);
        window = static_cast<Window*>(createStruct->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->m_hwnd = hwnd;
    } else {
        window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    // Forward to ImGui
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    if (window) {
        return window->HandleMessage(msg, wparam, lparam);
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

LRESULT Window::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_SIZE: {
        uint32_t width = LOWORD(lparam);
        uint32_t height = HIWORD(lparam);

        m_minimized = (wparam == SIZE_MINIMIZED);

        if (!m_minimized && (width != m_width || height != m_height)) {
            m_width = width;
            m_height = height;
            if (m_resizeCallback) {
                m_resizeCallback(m_width, m_height);
            }
            SE_LOG_INFO("Window resized: {}x{}", m_width, m_height);
        }
        return 0;
    }
    case WM_CLOSE:
        SavePlacement();
        DestroyWindow(m_hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(m_hwnd, msg, wparam, lparam);
}

} // namespace showcase
