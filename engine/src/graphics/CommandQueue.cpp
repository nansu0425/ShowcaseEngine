#include <showcase/graphics/CommandQueue.h>

#include <showcase/core/Log.h>
#include <showcase/core/Profiler.h>

namespace showcase {

// ── Init / Shutdown ──────────────────────────────────────────────────
bool CommandQueue::Init(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type) {
    SE_ZONE_SCOPED;
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    if (FAILED(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_queue)))) {
        SE_LOG_ERROR("Failed to create command queue");
        return false;
    }

    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)))) {
        SE_LOG_ERROR("Failed to create fence");
        return false;
    }

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) {
        SE_LOG_ERROR("Failed to create fence event");
        return false;
    }

    m_fenceValue = 0;
    return true;
}

void CommandQueue::Shutdown() {
    Flush();
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
    m_fence.Reset();
    m_queue.Reset();
}

// ── Execution ────────────────────────────────────────────────────────
uint64_t CommandQueue::ExecuteCommandList(ID3D12CommandList* commandList) {
    SE_ZONE_SCOPED_C(profile::kColorGPUSync);
    m_queue->ExecuteCommandLists(1, &commandList);
    return Signal();
}

uint64_t CommandQueue::Signal() {
    m_fenceValue++;
    m_queue->Signal(m_fence.Get(), m_fenceValue);
    return m_fenceValue;
}

// ── Synchronization ─────────────────────────────────────────────────
void CommandQueue::WaitForFenceValue(uint64_t fenceValue) {
    SE_ZONE_SCOPED_C(profile::kColorGPUSync);
    if (!IsFenceComplete(fenceValue)) {
        m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void CommandQueue::Flush() {
    SE_ZONE_SCOPED_C(profile::kColorGPUSync);
    WaitForFenceValue(Signal());
}

bool CommandQueue::IsFenceComplete(uint64_t fenceValue) const {
    return m_fence->GetCompletedValue() >= fenceValue;
}

} // namespace showcase
