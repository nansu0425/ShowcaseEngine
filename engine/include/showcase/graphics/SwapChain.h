#pragma once

#include <showcase/graphics/DescriptorHeap.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace showcase {

class CommandQueue;

struct SwapChainDesc {
    ID3D12Device* device = nullptr;
    IDXGIFactory6* factory = nullptr;
    CommandQueue* commandQueue = nullptr;
    HWND hwnd = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
};

class SwapChain {
public:
    static constexpr uint32_t kBufferCount = 3;

    [[nodiscard]] bool Init(const SwapChainDesc& desc);
    void Shutdown();

    void Present(bool vsync = true);
    void Resize(ID3D12Device* device, uint32_t width, uint32_t height);

    ID3D12Resource* GetCurrentBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const;
    uint32_t GetCurrentBackBufferIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }
    DXGI_FORMAT GetFormat() const { return m_format; }

private:
    void CreateRenderTargetViews(ID3D12Device* device);

    ComPtr<IDXGISwapChain4> m_swapChain;
    std::array<ComPtr<ID3D12Resource>, kBufferCount> m_backBuffers;
    DescriptorHeap m_rtvHeap;
    DXGI_FORMAT m_format = DXGI_FORMAT_R8G8B8A8_UNORM;
};

} // namespace showcase
