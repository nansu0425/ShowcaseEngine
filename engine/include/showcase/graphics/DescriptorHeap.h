#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace showcase {

struct DescriptorHandle {
    D3D12_CPU_DESCRIPTOR_HANDLE cpu = {};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = {};
    uint32_t index = UINT32_MAX;

    bool IsValid() const { return index != UINT32_MAX; }
};

struct DescriptorHeapDesc {
    ID3D12Device* device = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE type = {};
    uint32_t numDescriptors = 0;
    bool shaderVisible = false;
};

class DescriptorHeap {
public:
    [[nodiscard]] bool Init(const DescriptorHeapDesc& desc);
    void Shutdown();

    DescriptorHandle Allocate();
    void Free(const DescriptorHandle& handle);

    DescriptorHandle GetHandle(uint32_t index) const;

    ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }
    D3D12_DESCRIPTOR_HEAP_TYPE GetType() const { return m_type; }
    uint32_t GetDescriptorSize() const { return m_descriptorSize; }
    bool IsShaderVisible() const { return m_shaderVisible; }

private:
    ComPtr<ID3D12DescriptorHeap> m_heap;
    D3D12_DESCRIPTOR_HEAP_TYPE m_type = {};
    uint32_t m_descriptorSize = 0;
    uint32_t m_numDescriptors = 0;
    bool m_shaderVisible = false;

    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart = {};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart = {};

    std::vector<uint32_t> m_freeIndices;
    uint32_t m_nextIndex = 0;
};

} // namespace showcase
