#include <showcase/ui/Viewport.h>
#include <showcase/core/Log.h>
#include <imgui.h>

namespace showcase {

bool Viewport::Init(ID3D12Device* device, D3D12MA::Allocator* allocator,
                     DescriptorHeap& srvHeap, CommandQueue& directQueue,
                     uint32_t initialWidth, uint32_t initialHeight) {
    m_device = device;
    m_allocator = allocator;
    m_srvHeap = &srvHeap;
    m_directQueue = &directQueue;

    if (!m_renderTarget.Init(device, allocator, srvHeap, initialWidth, initialHeight)) {
        return false;
    }

    if (!m_depthBuffer.Init(device, allocator, initialWidth, initialHeight)) {
        return false;
    }

    m_width = initialWidth;
    m_height = initialHeight;

    SE_LOG_INFO("Viewport initialized ({}x{})", initialWidth, initialHeight);
    return true;
}

void Viewport::Shutdown(DescriptorHeap& srvHeap) {
    m_renderTarget.Shutdown(srvHeap);
    m_depthBuffer.Shutdown();
}

void Viewport::BeginRender(CommandList& cmdList) {
    // Apply deferred resize
    if (m_resizePending) {
        m_resizePending = false;
        Resize(m_pendingWidth, m_pendingHeight);
    }

    // Transition render target: SRV -> RT
    cmdList.TransitionBarrier(m_renderTarget.GetResource(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    auto rtv = m_renderTarget.GetRTV();
    auto dsv = m_depthBuffer.GetDSV();
    float clearColor[] = {0.05f, 0.05f, 0.08f, 1.0f};
    auto* cmd = cmdList.Get();
    cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    D3D12_VIEWPORT vp = {};
    vp.Width = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    cmd->RSSetViewports(1, &vp);

    D3D12_RECT scissor = {0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height)};
    cmd->RSSetScissorRects(1, &scissor);
}

void Viewport::EndRender(CommandList& cmdList) {
    // Transition render target: RT -> SRV
    cmdList.TransitionBarrier(m_renderTarget.GetResource(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void Viewport::OnImGui(float fps, float deltaTime) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("Viewport")) {
        ImVec2 size = ImGui::GetContentRegionAvail();
        uint32_t w = static_cast<uint32_t>(size.x);
        uint32_t h = static_cast<uint32_t>(size.y);

        // Clamp to minimum 1x1
        w = (w < 1) ? 1 : w;
        h = (h < 1) ? 1 : h;

        // Detect resize
        if (w != m_width || h != m_height) {
            m_pendingWidth = w;
            m_pendingHeight = h;
            m_resizePending = true;
        }

        // Display render target
        auto gpuHandle = m_renderTarget.GetSRVHandle().gpu;
        ImGui::Image((ImTextureID)gpuHandle.ptr, ImVec2(static_cast<float>(m_width), static_cast<float>(m_height)));

        // FPS overlay drawn directly on the viewport's draw list
        if (m_showFPS) {
            ImVec2 imageMin = ImGui::GetItemRectMin();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 textPos(imageMin.x + 10, imageMin.y + 10);
            char fpsText[64];
            snprintf(fpsText, sizeof(fpsText), "FPS: %.1f\nFrame: %.2f ms", fps, deltaTime * 1000.0f);
            ImVec2 textSize = ImGui::CalcTextSize(fpsText);
            ImVec2 padding(6, 4);
            drawList->AddRectFilled(
                ImVec2(textPos.x - padding.x, textPos.y - padding.y),
                ImVec2(textPos.x + textSize.x + padding.x, textPos.y + textSize.y + padding.y),
                IM_COL32(0, 0, 0, 128), 4.0f);
            drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), fpsText);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void Viewport::Resize(uint32_t width, uint32_t height) {
    m_directQueue->Flush();
    m_renderTarget.Resize(m_device, m_allocator, *m_srvHeap, width, height);
    m_depthBuffer.Resize(m_device, m_allocator, width, height);
    m_width = width;
    m_height = height;

    if (m_resizeCallback) {
        m_resizeCallback(width, height);
    }
}

} // namespace showcase
