#include <showcase/graphics/OffscreenTarget.h>
#include <showcase/graphics/RenderContext.h>
#include <showcase/core/Log.h>

namespace showcase {

bool OffscreenTarget::Init(RenderContext& ctx, uint32_t initialWidth, uint32_t initialHeight) {
    m_ctx = &ctx;

    auto* device = ctx.GetDevice().GetDevice();
    auto* allocator = ctx.GetDevice().GetAllocator();

    if (!m_renderTarget.Init(device, allocator, ctx.GetSrvHeap(), initialWidth, initialHeight)) {
        return false;
    }

    if (!m_depthBuffer.Init(device, allocator, initialWidth, initialHeight)) {
        return false;
    }

    m_width = initialWidth;
    m_height = initialHeight;

    SE_LOG_INFO("OffscreenTarget initialized ({}x{})", initialWidth, initialHeight);
    return true;
}

void OffscreenTarget::Shutdown(RenderContext& ctx) {
    m_renderTarget.Shutdown(ctx.GetSrvHeap());
    m_depthBuffer.Shutdown();
}

void OffscreenTarget::BeginRender(CommandList& cmdList) {
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

void OffscreenTarget::EndRender(CommandList& cmdList) {
    // Transition render target: RT -> SRV
    cmdList.TransitionBarrier(m_renderTarget.GetResource(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void OffscreenTarget::RequestResize(uint32_t width, uint32_t height) {
    if (width != m_width || height != m_height) {
        m_pendingWidth = width;
        m_pendingHeight = height;
        m_resizePending = true;
    }
}

void OffscreenTarget::Resize(uint32_t width, uint32_t height) {
    auto* device = m_ctx->GetDevice().GetDevice();
    auto* allocator = m_ctx->GetDevice().GetAllocator();
    m_ctx->GetDirectQueue().Flush();
    m_renderTarget.Resize(device, allocator, m_ctx->GetSrvHeap(), width, height);
    m_depthBuffer.Resize(device, allocator, width, height);
    m_width = width;
    m_height = height;

    if (m_resizeCallback) {
        m_resizeCallback(width, height);
    }
}

} // namespace showcase
