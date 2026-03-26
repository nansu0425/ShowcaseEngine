#pragma once

#include <cstdint>
#include <string>

namespace showcase {

// Returns the directory containing the running executable, with trailing separator.
std::string GetExecutableDir();

// Returns the last-write time of a file as a Win32 FILETIME packed into uint64_t.
// Returns 0 if the file does not exist or cannot be queried.
uint64_t GetFileLastWriteTime(const std::string& path);

// Returns a FNV-1a 64-bit hash of the file contents, or 0 on failure.
uint64_t HashFileContents(const std::string& path);

} // namespace showcase
