#include <showcase/core/Window.h>

#include <showcase/core/Log.h>

#include <nlohmann/json.hpp>

#include <fstream>

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

    m_hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        desc.title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr, nullptr,
        wc.hInstance,
        this
    );

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
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

// ── State persistence ─────────────────────────────────────────────
void Window::SavePlacement() const {
    if (!m_hwnd) return;

    WINDOWPLACEMENT wp = {};
    wp.length = sizeof(WINDOWPLACEMENT);
    if (!GetWindowPlacement(m_hwnd, &wp)) return;

    nlohmann::json j;
    j["showCmd"] = wp.showCmd;
    j["normalPosition"] = {
        {"left",   wp.rcNormalPosition.left},
        {"top",    wp.rcNormalPosition.top},
        {"right",  wp.rcNormalPosition.right},
        {"bottom", wp.rcNormalPosition.bottom}
    };

    std::ofstream file(GetConfigPath());
    if (file.is_open()) {
        file << j.dump(2);
        SE_LOG_INFO("Window placement saved");
    }
}

void Window::RestorePlacement() {
    if (!m_hwnd) return;

    std::ifstream file(GetConfigPath());
    if (!file.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    if (!nlohmann::json::accept(content)) {
        SE_LOG_WARN("Failed to parse window config");
        return;
    }
    nlohmann::json j = nlohmann::json::parse(content);

    WINDOWPLACEMENT wp = {};
    wp.length = sizeof(WINDOWPLACEMENT);

    auto& np = j["normalPosition"];
    wp.rcNormalPosition.left   = np["left"].get<LONG>();
    wp.rcNormalPosition.top    = np["top"].get<LONG>();
    wp.rcNormalPosition.right  = np["right"].get<LONG>();
    wp.rcNormalPosition.bottom = np["bottom"].get<LONG>();
    wp.showCmd = j["showCmd"].get<UINT>();

    SetWindowPlacement(m_hwnd, &wp);
    SE_LOG_INFO("Window placement restored");
}

// ── Message processing ────────────────────────────────────────────
bool Window::ProcessMessages() {
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
