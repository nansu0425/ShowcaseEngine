#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <D3D12MemAlloc.h>

using Microsoft::WRL::ComPtr;

namespace showcase {

class Device {
public:
    bool Init(bool enableDebugLayer = true);
    void Shutdown();

    ID3D12Device* GetDevice() const { return m_device.Get(); }
    IDXGIFactory6* GetFactory() const { return m_factory.Get(); }
    D3D12MA::Allocator* GetAllocator() const { return m_allocator.Get(); }

    bool SupportsRayTracing() const { return m_supportsRayTracing; }
    bool SupportsMeshShaders() const { return m_supportsMeshShaders; }

private:
    ComPtr<IDXGIFactory6> m_factory;
    ComPtr<IDXGIAdapter4> m_adapter;
    ComPtr<ID3D12Device> m_device;
    ComPtr<D3D12MA::Allocator> m_allocator;
    ComPtr<ID3D12InfoQueue1> m_infoQueue;

    bool m_supportsRayTracing = false;
    bool m_supportsMeshShaders = false;
};

} // namespace showcase
