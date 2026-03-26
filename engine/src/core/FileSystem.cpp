#include <showcase/core/FileSystem.h>

#include <Windows.h>

namespace showcase {

std::string GetExecutableDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string exePath(path);
    return exePath.substr(0, exePath.find_last_of("\\/") + 1);
}

uint64_t GetFileLastWriteTime(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA attr = {};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attr))
        return 0;
    return (static_cast<uint64_t>(attr.ftLastWriteTime.dwHighDateTime) << 32) | attr.ftLastWriteTime.dwLowDateTime;
}

uint64_t HashFileContents(const std::string& path) {
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return 0;

    constexpr uint64_t kFnvOffset = 14695981039346656037ULL;
    constexpr uint64_t kFnvPrime = 1099511628211ULL;
    uint64_t hash = kFnvOffset;

    uint8_t buf[8192];
    DWORD bytesRead = 0;
    while (ReadFile(hFile, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0) {
        for (DWORD i = 0; i < bytesRead; ++i) {
            hash ^= buf[i];
            hash *= kFnvPrime;
        }
    }

    CloseHandle(hFile);
    return hash;
}

} // namespace showcase
