#include <showcase/core/Platform.h>

#include <commdlg.h>
#include <shellapi.h>
#include <Windows.h>

namespace showcase {

std::string GetExecutableDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string exePath(path);
    return exePath.substr(0, exePath.find_last_of("\\/") + 1);
}

void SleepMs(uint32_t ms) {
    Sleep(static_cast<DWORD>(ms));
}

void ShowErrorDialog(const wchar_t* title, const wchar_t* message) {
    MessageBoxW(nullptr, message, title, MB_OK | MB_ICONERROR);
}

DialogResult ShowConfirmDialog(void* ownerHwnd, const char* title, const char* message) {
    int result = MessageBoxA(static_cast<HWND>(ownerHwnd), message, title, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (result == IDYES)
        return DialogResult::Yes;
    if (result == IDNO)
        return DialogResult::No;
    return DialogResult::Cancel;
}

void ShowErrorMessage(void* ownerHwnd, const char* title, const char* message) {
    MessageBoxA(static_cast<HWND>(ownerHwnd), message, title, MB_OK | MB_ICONERROR);
}

std::string OpenFileDialog(void* ownerHwnd, const char* filter, const char* defaultExt) {
    char filePath[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(ownerHwnd);
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = defaultExt;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(filePath);
    }
    return {};
}

std::string SaveFileDialog(void* ownerHwnd, const char* filter, const char* defaultExt) {
    char filePath[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(ownerHwnd);
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = defaultExt;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn)) {
        return std::string(filePath);
    }
    return {};
}

bool LaunchProcess(const std::string& exePath) {
    HINSTANCE result = ShellExecuteA(nullptr, "open", exePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
}

} // namespace showcase
