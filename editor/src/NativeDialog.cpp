#include <showcase/editor/NativeDialog.h>

// clang-format off
#include <Windows.h>
#include <commdlg.h>
// clang-format on

namespace showcase {

DialogResult ShowConfirmDialog(void* ownerHwnd, const char* title, const char* message) {
    int result = MessageBoxA(static_cast<HWND>(ownerHwnd), message, title, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (result == IDYES)
        return DialogResult::Yes;
    if (result == IDNO)
        return DialogResult::No;
    return DialogResult::Cancel;
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

} // namespace showcase
