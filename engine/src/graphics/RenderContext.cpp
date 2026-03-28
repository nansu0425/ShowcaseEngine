#include <showcase/graphics/RenderContext.h>

#include <showcase/core/Log.h>
#include <showcase/core/Profiler.h>

namespace showcase {

// ── Init / Shutdown ──────────────────────────────────────────────────
bool RenderContext::Init(HWND hwnd, uint32_t width, uint32_t height) {
    SE_ZONE_SCOPED;
#ifdef _DEBUG
    bool enableDebug = true;
#else
    bool enableDebug = false;
#endif

    if (!m_device.Init(enableDebug))
        return false;

    auto* device = m_device.GetDevice();

    if (!m_directQueue.Init(device, D3D12_COMMAND_LIST_TYPE_DIRECT))
        return false;
    if (!m_swapChain.Init({device, m_device.GetFactory(), &m_directQueue, hwnd, width, height}))
        return false;

    // Shader-visible SRV heap (for ImGui and general use)
    if (!m_srvHeap.Init({device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, true}))
        return false;

    if (!m_depthBuffer.Init({device, m_device.GetAllocator(), width, height}))
        return false;
    m_depthBuffer.GetResource()->SetName(L"BackBuffer DepthBuffer");

    if (!m_frameResource.Init(device))
        return false;
    if (!m_commandList.Init(device, D3D12_COMMAND_LIST_TYPE_DIRECT))
        return false;

    m_width = width;
    m_height = height;
    m_currentFrameIndex = m_swapChain.GetCurrentBackBufferIndex();

    SE_LOG_INFO("Render context initialized");
    return true;
}

void RenderContext::Shutdown() {
    SE_ZONE_SCOPED;
    m_directQueue.Flush();
    m_depthBuffer.Shutdown(m_srvHeap);
    m_shaderManager.Clear();
    m_commandList.Shutdown();
    m_frameResource.Shutdown();
    m_srvHeap.Shutdown();
    m_swapChain.Shutdown();
    m_directQueue.Shutdown();
    m_device.Shutdown();
}

// ── Frame management ─────────────────────────────────────────────────
void RenderContext::BeginFrame() {
    SE_ZONE_SCOPED_C(profile::kColorRendering);

    m_currentFrameIndex = m_swapChain.GetCurrentBackBufferIndex();

    {
        SE_ZONE_SCOPED_NC("WaitForFenceValue", profile::kColorGPUSync);
        // Wait for this frame's resources to be available
        uint64_t fenceValue = m_frameResource.GetFenceValue(m_currentFrameIndex);
        m_directQueue.WaitForFenceValue(fenceValue);
    }

    // Reset the command allocator and command list
    m_frameResource.BeginFrame(m_currentFrameIndex);
    auto* allocator = m_frameResource.GetAllocator(m_currentFrameIndex);
    m_commandList.Get()->Reset(allocator, nullptr);

    // Bind SRV heap once per frame so all passes can use descriptor tables
    ID3D12DescriptorHeap* heaps[] = {m_srvHeap.GetHeap()};
    m_commandList.Get()->SetDescriptorHeaps(1, heaps);
}

void RenderContext::EndFrame() {
    SE_ZONE_SCOPED_C(profile::kColorGPUSync);
    m_commandList.Close();

    // Execute and present
    uint64_t fenceValue = m_directQueue.ExecuteCommandList(m_commandList.Get());
    m_frameResource.SetFenceValue(m_currentFrameIndex, fenceValue);

    m_swapChain.Present(m_vsyncEnabled);
}

// ── Resize ───────────────────────────────────────────────────────────
void RenderContext::Resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0)
        return;

    m_width = width;
    m_height = height;
    m_directQueue.Flush();
    m_swapChain.Resize(m_device.GetDevice(), width, height);
    m_depthBuffer.Resize(m_device.GetDevice(), m_device.GetAllocator(), width, height);
    m_depthBuffer.GetResource()->SetName(L"BackBuffer DepthBuffer");
}

// ── Back buffer pass ─────────────────────────────────────────────────
void RenderContext::BeginBackBufferPass(const float clearColor[4]) {
    auto* cmdList = m_commandList.Get();

    // Transition back buffer to render target
    auto* backBuffer = m_swapChain.GetCurrentBackBuffer();
    m_commandList.TransitionBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // Clear and bind render target
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapChain.GetCurrentRTV();
    cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    // Set viewport
    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    cmdList->RSSetViewports(1, &viewport);

    // Set scissor rect
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height)};
    cmdList->RSSetScissorRects(1, &scissor);
}

void RenderContext::BeginBackBufferScenePass(const float clearColor[4]) {
    auto* cmdList = m_commandList.Get();

    // Transition back buffer to render target
    auto* backBuffer = m_swapChain.GetCurrentBackBuffer();
    m_commandList.TransitionBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // Clear render target and depth buffer
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapChain.GetCurrentRTV();
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_depthBuffer.GetDSV();
    cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    // Set viewport
    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    cmdList->RSSetViewports(1, &viewport);

    // Set scissor rect
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height)};
    cmdList->RSSetScissorRects(1, &scissor);
}

void RenderContext::EndBackBufferPass() {
    auto* backBuffer = m_swapChain.GetCurrentBackBuffer();
    m_commandList.TransitionBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
}

} // namespace showcase
