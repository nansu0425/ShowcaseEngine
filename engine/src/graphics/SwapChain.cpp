#include <showcase/graphics/SwapChain.h>
#include <showcase/graphics/CommandQueue.h>
#include <showcase/core/Log.h>

namespace showcase {

bool SwapChain::Init(ID3D12Device* device, IDXGIFactory6* factory, CommandQueue& commandQueue,
                      HWND hwnd, uint32_t width, uint32_t height) {
    // Create RTV descriptor heap
    if (!m_rtvHeap.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kBufferCount, false)) {
        return false;
    }

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = m_format;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = {1, 0};
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = kBufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    ComPtr<IDXGISwapChain1> swapChain1;
    if (FAILED(factory->CreateSwapChainForHwnd(
            commandQueue.GetQueue(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1))) {
        SE_LOG_ERROR("Failed to create swap chain");
        return false;
    }

    // Disable Alt+Enter fullscreen toggle
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    swapChain1.As(&m_swapChain);
    CreateRenderTargetViews(device);

    SE_LOG_INFO("Swap chain created: {}x{}, {} buffers", width, height, kBufferCount);
    return true;
}

void SwapChain::Shutdown() {
    for (auto& buffer : m_backBuffers) {
        buffer.Reset();
    }
    m_rtvHeap.Shutdown();
    m_swapChain.Reset();
}

void SwapChain::Present(bool vsync) {
    UINT syncInterval = vsync ? 1 : 0;
    UINT flags = vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING;
    m_swapChain->Present(syncInterval, flags);
}

void SwapChain::Resize(ID3D12Device* device, uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;

    for (auto& buffer : m_backBuffers) {
        buffer.Reset();
    }

    DXGI_SWAP_CHAIN_DESC1 desc;
    m_swapChain->GetDesc1(&desc);
    m_swapChain->ResizeBuffers(kBufferCount, width, height, desc.Format, desc.Flags);

    CreateRenderTargetViews(device);
    SE_LOG_INFO("Swap chain resized: {}x{}", width, height);
}

ID3D12Resource* SwapChain::GetCurrentBackBuffer() const {
    return m_backBuffers[m_swapChain->GetCurrentBackBufferIndex()].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::GetCurrentRTV() const {
    return m_rtvHeap.GetHandle(m_swapChain->GetCurrentBackBufferIndex()).cpu;
}

void SwapChain::CreateRenderTargetViews(ID3D12Device* device) {
    for (uint32_t i = 0; i < kBufferCount; i++) {
        m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = m_format;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        device->CreateRenderTargetView(m_backBuffers[i].Get(), &rtvDesc,
                                       m_rtvHeap.GetHandle(i).cpu);
    }
}

} // namespace showcase
