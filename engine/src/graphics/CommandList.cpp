#include <showcase/graphics/CommandList.h>
#include <showcase/core/Assert.h>
#include <showcase/core/Log.h>

namespace showcase {

// ── Init / Shutdown ──────────────────────────────────────────────────
bool CommandList::Init(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type) {
    m_type = type;

    if (FAILED(device->CreateCommandAllocator(type, IID_PPV_ARGS(&m_allocator)))) {
        SE_LOG_ERROR("Failed to create command allocator");
        return false;
    }

    if (FAILED(device->CreateCommandList(0, type, m_allocator.Get(), nullptr,
                                          IID_PPV_ARGS(&m_commandList)))) {
        SE_LOG_ERROR("Failed to create command list");
        return false;
    }

    // Start in closed state
    m_commandList->Close();
    return true;
}

void CommandList::Shutdown() {
    m_commandList.Reset();
    m_allocator.Reset();
}

// ── Frame management ─────────────────────────────────────────────────
void CommandList::Reset() {
    m_allocator->Reset();
    m_commandList->Reset(m_allocator.Get(), nullptr);
}

void CommandList::Close() {
    m_commandList->Close();
}

// ── Barriers ─────────────────────────────────────────────────────────
void CommandList::TransitionBarrier(ID3D12Resource* resource,
                                     D3D12_RESOURCE_STATES before,
                                     D3D12_RESOURCE_STATES after) {
    SE_ASSERT(resource != nullptr);
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_commandList->ResourceBarrier(1, &barrier);
}

} // namespace showcase
