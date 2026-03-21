#include <showcase/core/Application.h>
#include <showcase/core/Log.h>
#include <showcase/demo/ShowcaseRegistry.h>
#include <imgui_internal.h>

namespace showcase {

bool Application::Init(const ApplicationDesc& desc) {
    Log::Init();

    if (!m_window.Init(desc.window)) return false;

    if (!m_renderContext.Init(m_window.GetHandle(), m_window.GetWidth(), m_window.GetHeight())) {
        return false;
    }

    m_logConsole.Init();

    if (!m_imguiLayer.Init(m_window.GetHandle(), m_renderContext)) return false;

    if (!m_viewport.Init(
        m_renderContext.GetDevice().GetDevice(),
        m_renderContext.GetDevice().GetAllocator(),
        m_renderContext.GetSrvHeap(),
        m_renderContext.GetDirectQueue(),
        m_window.GetWidth(), m_window.GetHeight())) {
        return false;
    }

    m_viewport.SetResizeCallback([this](uint32_t w, uint32_t h) {
        m_showcaseManager.OnResize(w, h);
    });

    // Set resize callback
    m_window.SetResizeCallback([this](uint32_t w, uint32_t h) {
        m_resizePending = true;
        m_pendingWidth = w;
        m_pendingHeight = h;
    });

    m_timer.Reset();

    // Auto-load first available showcase
    const auto& entries = ShowcaseRegistry::Instance().GetAll();
    if (!entries.empty()) {
        m_showcaseManager.LoadShowcase(entries[0].name, m_renderContext);
    }

    SE_LOG_INFO("Application initialized");
    return true;
}

void Application::Shutdown() {
    m_renderContext.GetDirectQueue().Flush();
    m_showcaseManager.UnloadCurrent();
    m_viewport.Shutdown(m_renderContext.GetSrvHeap());
    m_imguiLayer.Shutdown();
    m_renderContext.Shutdown();
    m_window.Shutdown();
    SE_LOG_INFO("Application shutdown");
}

int Application::Run() {
    while (m_window.ProcessMessages()) {
        m_timer.Tick();
        m_input.Update();

        if (m_window.IsMinimized()) {
            Sleep(10);
            continue;
        }

        // Handle deferred resize
        if (m_resizePending) {
            m_resizePending = false;
            OnResize(m_pendingWidth, m_pendingHeight);
        }

        float dt = m_timer.DeltaTime();

        // Update
        m_showcaseManager.Update(dt);

        // Begin render frame
        m_renderContext.BeginFrame();
        auto* cmdList = m_renderContext.GetCommandList().Get();

        // Set SRV heap for showcase and ImGui rendering
        ID3D12DescriptorHeap* heaps[] = {m_renderContext.GetSrvHeap().GetHeap()};
        cmdList->SetDescriptorHeaps(1, heaps);

        // Phase 1: Render showcase to off-screen viewport render target
        m_viewport.BeginRender(m_renderContext.GetCommandList());
        m_showcaseManager.Render(m_renderContext);
        m_viewport.EndRender(m_renderContext.GetCommandList());

        // Phase 2: Render ImGui to back buffer
        auto* backBuffer = m_renderContext.GetSwapChain().GetCurrentBackBuffer();
        m_renderContext.GetCommandList().TransitionBarrier(backBuffer,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        auto rtv = m_renderContext.GetSwapChain().GetCurrentRTV();
        float clearColor[] = {0.05f, 0.05f, 0.08f, 1.0f};
        cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
        cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

        D3D12_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(m_window.GetWidth());
        viewport.Height = static_cast<float>(m_window.GetHeight());
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        cmdList->RSSetViewports(1, &viewport);

        D3D12_RECT scissor = {0, 0,
            static_cast<LONG>(m_window.GetWidth()),
            static_cast<LONG>(m_window.GetHeight())};
        cmdList->RSSetScissorRects(1, &scissor);

        // Render ImGui
        m_imguiLayer.BeginFrame();

        // DockBuilder must run BEFORE DockSpaceOverViewport
        ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
        static bool s_dockLayoutChecked = false;
        if (!s_dockLayoutChecked) {
            s_dockLayoutChecked = true;
            ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceId);
            if (!node || !node->IsSplitNode()) {
                ImGui::DockBuilderRemoveNode(dockspaceId);
                ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
                const ImGuiViewport* vp = ImGui::GetMainViewport();
                ImGui::DockBuilderSetNodeSize(dockspaceId, vp->Size);

                ImGuiID dockBottom;
                ImGuiID dockCenter;
                ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Down, 0.25f, &dockBottom, &dockCenter);
                ImGui::DockBuilderDockWindow("Log Console", dockBottom);
                ImGui::DockBuilderDockWindow("Viewport", dockCenter);
                ImGui::DockBuilderFinish(dockspaceId);
            }
        }

        // Create full-screen DockSpace
        ImGui::DockSpaceOverViewport(dockspaceId, ImGui::GetMainViewport());

        m_viewport.OnImGui(m_timer.FPS(), m_timer.DeltaTime());
        m_showcaseManager.RenderUI();
        m_logConsole.Render();
        m_imguiLayer.EndFrame(cmdList);

        // Transition back buffer to present
        m_renderContext.GetCommandList().TransitionBarrier(backBuffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

        // End frame
        m_renderContext.EndFrame();
    }

    return 0;
}

void Application::OnResize(uint32_t width, uint32_t height) {
    m_renderContext.Resize(width, height);
    SE_LOG_INFO("Application resized: {}x{}", width, height);
}

} // namespace showcase
