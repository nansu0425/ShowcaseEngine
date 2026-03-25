#include <showcase/core/FileSystem.h>

#include <Windows.h>

namespace showcase {

std::string GetExecutableDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string exePath(path);
    return exePath.substr(0, exePath.find_last_of("\\/") + 1);
}

} // namespace showcase
