#include <showcase/graphics/ShaderManager.h>

#include <showcase/core/Log.h>

#include <Windows.h>

#include <fstream>

namespace showcase {

static std::string GetExecutableDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string exePath(path);
    return exePath.substr(0, exePath.find_last_of("\\/") + 1);
}

// ── Shader loading ───────────────────────────────────────────────────
D3D12_SHADER_BYTECODE ShaderManager::LoadShader(const std::string& path) {
    // Check cache
    auto it = m_shaderCache.find(path);
    if (it != m_shaderCache.end()) {
        return {it->second.data(), it->second.size()};
    }

    // Resolve relative paths against executable directory
    std::string fullPath = path;
    if (path.size() < 2 || (path[0] != '/' && path[0] != '\\' && path[1] != ':')) {
        fullPath = GetExecutableDir() + path;
    }

    // Load from file
    std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        SE_LOG_ERROR("Failed to open shader file: {}", fullPath);
        return {};
    }

    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(fileSize));
    file.read(reinterpret_cast<char*>(data.data()), fileSize);

    auto [insertIt, inserted] = m_shaderCache.emplace(path, std::move(data));
    SE_LOG_INFO("Loaded shader: {} ({} bytes)", fullPath, insertIt->second.size());

    return {insertIt->second.data(), insertIt->second.size()};
}

// ── Lifecycle ────────────────────────────────────────────────────────
void ShaderManager::Clear() {
    m_shaderCache.clear();
}

} // namespace showcase
