#include <showcase/graphics/Device.h>

#include <showcase/core/Log.h>

#include <stdexcept>

namespace showcase {

// ── Init / Shutdown ──────────────────────────────────────────────────
bool Device::Init(bool enableDebugLayer) {
    UINT dxgiFactoryFlags = 0;

    // Enable debug layer
    if (enableDebugLayer) {
        ComPtr<ID3D12Debug3> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            SE_LOG_INFO("D3D12 debug layer enabled");
        }
    }

    // Create DXGI factory
    if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)))) {
        SE_LOG_ERROR("Failed to create DXGI factory");
        return false;
    }

    // Find best adapter (prefer discrete GPU)
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0;
         m_factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
         ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                                         IID_PPV_ARGS(&m_device)))) {
            adapter.As(&m_adapter);

            char adapterName[256];
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, adapterName, 256, nullptr, nullptr);
            SE_LOG_INFO("GPU: {}", adapterName);
            SE_LOG_INFO("Video Memory: {} MB", desc.DedicatedVideoMemory / (1024 * 1024));
            break;
        }
    }

    if (!m_device) {
        SE_LOG_ERROR("Failed to create D3D12 device");
        return false;
    }

    // Configure debug info queue
    if (enableDebugLayer) {
        if (SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&m_infoQueue)))) {
            m_infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            m_infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

            D3D12_MESSAGE_SEVERITY suppressSeverities[] = {
                D3D12_MESSAGE_SEVERITY_INFO
            };
            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumSeverities = _countof(suppressSeverities);
            filter.DenyList.pSeverityList = suppressSeverities;
            m_infoQueue->PushStorageFilter(&filter);
        }
    }

    // Create D3D12MA allocator
    D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
    allocatorDesc.pDevice = m_device.Get();
    allocatorDesc.pAdapter = m_adapter.Get();
    allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;

    if (FAILED(D3D12MA::CreateAllocator(&allocatorDesc, &m_allocator))) {
        SE_LOG_ERROR("Failed to create D3D12 Memory Allocator");
        return false;
    }

    // Check feature support
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    if (SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)))) {
        m_supportsRayTracing = (options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
    if (SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7)))) {
        m_supportsMeshShaders = (options7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED);
    }

    SE_LOG_INFO("Ray Tracing: {}", m_supportsRayTracing ? "Supported" : "Not Supported");
    SE_LOG_INFO("Mesh Shaders: {}", m_supportsMeshShaders ? "Supported" : "Not Supported");

    return true;
}

void Device::Shutdown() {
    m_allocator.Reset();
    m_infoQueue.Reset();
    m_device.Reset();
    m_adapter.Reset();
    m_factory.Reset();
}

} // namespace showcase
