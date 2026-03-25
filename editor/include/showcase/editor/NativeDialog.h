#pragma once

#include <string>

namespace showcase {

enum class DialogResult {
    Yes,
    No,
    Cancel
};

DialogResult ShowConfirmDialog(void* ownerHwnd, const char* title, const char* message);

// Native file dialogs. Returns selected file path, or empty string if cancelled.
std::string OpenFileDialog(void* ownerHwnd, const char* filter, const char* defaultExt);
std::string SaveFileDialog(void* ownerHwnd, const char* filter, const char* defaultExt);

} // namespace showcase
