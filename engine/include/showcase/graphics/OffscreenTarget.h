#pragma once

#include <showcase/graphics/RenderTarget.h>
#include <showcase/graphics/DepthBuffer.h>
#include <showcase/graphics/CommandList.h>
#include <cstdint>
#include <functional>

namespace showcase {

class RenderContext;

using ResizeCallback = std::function<void(uint32_t, uint32_t)>;

class OffscreenTarget {
public:
    [[nodiscard]] bool Init(RenderContext& ctx, uint32_t initialWidth, uint32_t initialHeight);
    void Shutdown(RenderContext& ctx);

    void BeginRender(CommandList& cmdList);
    void EndRender(CommandList& cmdList);

    void RequestResize(uint32_t width, uint32_t height);

    void SetResizeCallback(ResizeCallback callback) { m_resizeCallback = std::move(callback); }

    RenderTarget& GetRenderTarget() { return m_renderTarget; }
    const RenderTarget& GetRenderTarget() const { return m_renderTarget; }

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    float GetAspectRatio() const { return m_height > 0 ? static_cast<float>(m_width) / m_height : 1.0f; }

private:
    void Resize(uint32_t width, uint32_t height);

    RenderContext* m_ctx = nullptr;

    RenderTarget m_renderTarget;
    DepthBuffer m_depthBuffer;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    bool m_resizePending = false;
    uint32_t m_pendingWidth = 0;
    uint32_t m_pendingHeight = 0;

    ResizeCallback m_resizeCallback;
};

} // namespace showcase
