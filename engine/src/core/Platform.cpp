#include <showcase/core/Platform.h>

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

} // namespace showcase
