#pragma once

#include <d3d12.h>
#include <cstdint>

struct HWND__;
typedef HWND__* HWND;

namespace showcase {

class RenderContext;

class ImGuiLayer {
public:
    bool Init(HWND hwnd, RenderContext& ctx);
    void Shutdown();

    void BeginFrame();
    void EndFrame(ID3D12GraphicsCommandList* commandList);

private:
    uint32_t m_srvDescriptorIndex = 0;
};

} // namespace showcase
