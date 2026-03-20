#include <showcase/graphics/RenderContext.h>
#include <showcase/core/Log.h>

namespace showcase {

bool RenderContext::Init(HWND hwnd, uint32_t width, uint32_t height) {
#ifdef _DEBUG
    bool enableDebug = true;
#else
    bool enableDebug = false;
#endif

    if (!m_device.Init(enableDebug)) return false;

    auto* device = m_device.GetDevice();

    if (!m_directQueue.Init(device, D3D12_COMMAND_LIST_TYPE_DIRECT)) return false;
    if (!m_swapChain.Init(device, m_device.GetFactory(), m_directQueue, hwnd, width, height)) return false;

    // Shader-visible SRV heap (for ImGui and general use)
    if (!m_srvHeap.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, true)) return false;

    if (!m_depthBuffer.Init(device, m_device.GetAllocator(), width, height)) return false;

    if (!m_frameResource.Init(device)) return false;
    if (!m_commandList.Init(device, D3D12_COMMAND_LIST_TYPE_DIRECT)) return false;

    m_currentFrameIndex = m_swapChain.GetCurrentBackBufferIndex();

    SE_LOG_INFO("Render context initialized");
    return true;
}

void RenderContext::Shutdown() {
    m_directQueue.Flush();
    m_depthBuffer.Shutdown();
    m_shaderManager.Clear();
    m_commandList.Shutdown();
    m_frameResource.Shutdown();
    m_srvHeap.Shutdown();
    m_swapChain.Shutdown();
    m_directQueue.Shutdown();
    m_device.Shutdown();
}

void RenderContext::BeginFrame() {
    m_currentFrameIndex = m_swapChain.GetCurrentBackBufferIndex();

    // Wait for this frame's resources to be available
    auto fenceValue = m_frameResource.GetFenceValue(m_currentFrameIndex);
    m_directQueue.WaitForFenceValue(fenceValue);

    // Reset the command allocator and command list
    m_frameResource.BeginFrame(m_currentFrameIndex);
    auto* allocator = m_frameResource.GetAllocator(m_currentFrameIndex);
    m_commandList.Get()->Reset(allocator, nullptr);
}

void RenderContext::EndFrame() {
    m_commandList.Close();

    // Execute and present
    auto fenceValue = m_directQueue.ExecuteCommandList(m_commandList.Get());
    m_frameResource.SetFenceValue(m_currentFrameIndex, fenceValue);

    m_swapChain.Present(true);
}

void RenderContext::Resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;

    m_directQueue.Flush();
    m_swapChain.Resize(m_device.GetDevice(), width, height);
    m_depthBuffer.Resize(m_device.GetDevice(), m_device.GetAllocator(), width, height);
}

} // namespace showcase
