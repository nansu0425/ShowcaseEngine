#pragma once

#include <cstdint>

struct HWND__;
using HWND = HWND__*;

namespace showcase {

class RenderContext;
class CommandList;

class ImGuiLayer {
public:
    [[nodiscard]] bool Init(HWND hwnd, RenderContext& ctx);
    void Shutdown();

    void BeginFrame();
    void EndFrame(CommandList& cmdList);

private:
    uint32_t m_srvDescriptorIndex = 0;
};

} // namespace showcase
