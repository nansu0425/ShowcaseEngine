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

} // namespace showcase
