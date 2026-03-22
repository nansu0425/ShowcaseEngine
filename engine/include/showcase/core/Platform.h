#pragma once

#include <string>
#include <cstdint>

namespace showcase {

// Returns the directory containing the running executable, with trailing separator.
std::string GetExecutableDir();

// Suspends execution for the given number of milliseconds.
void SleepMs(uint32_t ms);

// Displays a modal error dialog box.
void ShowErrorDialog(const wchar_t* title, const wchar_t* message);

// Modal dialog boxes.
enum class DialogResult { Yes, No, Cancel };
DialogResult ShowConfirmDialog(void* ownerHwnd, const char* title, const char* message);
void ShowErrorMessage(void* ownerHwnd, const char* title, const char* message);

// Native file dialogs. Returns selected file path, or empty string if cancelled.
std::string OpenFileDialog(void* ownerHwnd, const char* filter, const char* defaultExt);
std::string SaveFileDialog(void* ownerHwnd, const char* filter, const char* defaultExt);

} // namespace showcase
