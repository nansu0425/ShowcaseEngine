#include <showcase/graphics/DescriptorHeap.h>
#include <showcase/core/Assert.h>
#include <showcase/core/Log.h>

namespace showcase {

// ── Init / Shutdown ──────────────────────────────────────────────────
bool DescriptorHeap::Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
                           uint32_t numDescriptors, bool shaderVisible) {
    m_type = type;
    m_numDescriptors = numDescriptors;
    m_shaderVisible = shaderVisible;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = numDescriptors;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                               : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask = 0;

    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)))) {
        SE_LOG_ERROR("Failed to create descriptor heap");
        return false;
    }

    m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);
    m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();
    if (shaderVisible) {
        m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
    }

    m_nextIndex = 0;
    return true;
}

void DescriptorHeap::Shutdown() {
    m_heap.Reset();
    m_freeIndices.clear();
    m_nextIndex = 0;
}

// ── Allocation ───────────────────────────────────────────────────────
DescriptorHandle DescriptorHeap::Allocate() {
    uint32_t index;
    if (!m_freeIndices.empty()) {
        index = m_freeIndices.back();
        m_freeIndices.pop_back();
    } else {
        if (m_nextIndex >= m_numDescriptors) {
            SE_LOG_ERROR("Descriptor heap out of space");
            return {};
        }
        index = m_nextIndex++;
    }

    return GetHandle(index);
}

void DescriptorHeap::Free(const DescriptorHandle& handle) {
    if (handle.IsValid()) {
        m_freeIndices.push_back(handle.index);
    }
}

// ── Query ────────────────────────────────────────────────────────────
DescriptorHandle DescriptorHeap::GetHandle(uint32_t index) const {
    SE_ASSERT(index < m_numDescriptors);
    DescriptorHandle handle;
    handle.index = index;
    handle.cpu.ptr = m_cpuStart.ptr + static_cast<SIZE_T>(index) * m_descriptorSize;
    if (m_shaderVisible) {
        handle.gpu.ptr = m_gpuStart.ptr + static_cast<UINT64>(index) * m_descriptorSize;
    }
    return handle;
}

} // namespace showcase
