#pragma once

#include <showcase/graphics/RenderTarget.h>
#include <showcase/graphics/DepthBuffer.h>
#include <showcase/graphics/CommandList.h>
#include <showcase/graphics/CommandQueue.h>
#include <cstdint>
#include <functional>

namespace showcase {

using ViewportResizeCallback = std::function<void(uint32_t, uint32_t)>;

class Viewport {
public:
    bool Init(ID3D12Device* device, D3D12MA::Allocator* allocator,
              DescriptorHeap& srvHeap, CommandQueue& directQueue,
              uint32_t initialWidth, uint32_t initialHeight);
    void Shutdown(DescriptorHeap& srvHeap);

    void BeginRender(CommandList& cmdList);
    void EndRender(CommandList& cmdList);
    void OnImGui();

    void SetResizeCallback(ViewportResizeCallback callback) { m_resizeCallback = std::move(callback); }

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    float GetAspectRatio() const { return m_height > 0 ? static_cast<float>(m_width) / m_height : 1.0f; }

private:
    void Resize(uint32_t width, uint32_t height);

    ID3D12Device* m_device = nullptr;
    D3D12MA::Allocator* m_allocator = nullptr;
    DescriptorHeap* m_srvHeap = nullptr;
    CommandQueue* m_directQueue = nullptr;

    RenderTarget m_renderTarget;
    DepthBuffer m_depthBuffer;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    bool m_resizePending = false;
    uint32_t m_pendingWidth = 0;
    uint32_t m_pendingHeight = 0;

    ViewportResizeCallback m_resizeCallback;
};

} // namespace showcase
