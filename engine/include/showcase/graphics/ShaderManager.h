#pragma once

#include <d3d12.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace showcase {

class ShaderManager {
public:
    D3D12_SHADER_BYTECODE LoadShader(const std::string& path);
    void Clear();

private:
    std::unordered_map<std::string, std::vector<uint8_t>> m_shaderCache;
};

} // namespace showcase
