#include <showcase/core/Application.h>
#include <showcase/core/Log.h>
#include <imgui_internal.h>
#include <filesystem>

namespace showcase {

using namespace DirectX::SimpleMath;
using namespace DirectX;

bool Application::Init(const ApplicationDesc& desc) {
    Log::Init();

    if (!m_window.Init(desc.window)) return false;

    if (!m_renderContext.Init(m_window.GetHandle(), m_window.GetWidth(), m_window.GetHeight())) {
        return false;
    }

    m_console.Init();

    m_console.RegisterCommand("fps", [this](const std::string&) -> std::string {
        m_viewport.ToggleShowFPS();
        return m_viewport.GetShowFPS() ? "FPS overlay enabled" : "FPS overlay disabled";
    });

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
        m_sceneRenderer.OnResize(w, h, m_camera);
    });

    m_viewport.SetToolbarCallback([this]() {
        m_editorController.RenderToolbar(m_camera, m_cameraController);
    });

    m_renderContext.SetViewport(&m_viewport);

    // Set resize callback
    m_window.SetResizeCallback([this](uint32_t w, uint32_t h) {
        m_resizePending = true;
        m_pendingWidth = w;
        m_pendingHeight = h;
    });

    m_window.RestorePlacement();

    // Initialize scene renderer
    m_sceneRenderer.Init(m_renderContext);

    // Build scene
    BuildDefaultScene();

    // Camera initial position
    m_camera.SetPosition(Vector3(0.0f, 5.0f, -15.0f));
    m_camera.SetLookAt(Vector3(0.0f, 0.0f, 0.0f));
    float aspectRatio = m_window.GetHeight() > 0
        ? static_cast<float>(m_window.GetWidth()) / m_window.GetHeight() : 16.0f / 9.0f;
    m_camera.SetPerspective(XM_PIDIV4, aspectRatio, 0.1f, 1000.0f);
    m_camera.UpdateViewMatrix();

    m_timer.Reset();

    SE_LOG_INFO("Application initialized");
    return true;
}

void Application::BuildDefaultScene() {
    auto* device = m_renderContext.GetDevice().GetDevice();
    auto* allocator = m_renderContext.GetDevice().GetAllocator();

    m_sceneRenderer.CreateGridModel(device, allocator, m_gridModel);
    m_sceneRenderer.CreateCubeModel(device, allocator, m_cubeModel);

    m_scene.Clear();
    m_scene.AddObject(&m_gridModel, "Ground Grid", Vector3(0, 0, 0));
    m_scene.AddObject(&m_cubeModel, "Cube 1", Vector3(0, 1, 0), Vector3(0, 0, 0), Vector3(2, 2, 2));
    m_scene.AddObject(&m_cubeModel, "Cube 2", Vector3(5, 0.75f, 3), Vector3(0, 0, 0), Vector3(1.5f, 1.5f, 1.5f));
    m_scene.AddObject(&m_cubeModel, "Cube 3", Vector3(-6, 2, -4), Vector3(0, 0, 0), Vector3(3, 4, 3));
    m_scene.AddObject(&m_cubeModel, "Cube 4", Vector3(8, 0.5f, -7), Vector3(0, 0, 0), Vector3(1, 1, 1));
    m_scene.AddObject(&m_cubeModel, "Cube 5", Vector3(-3, 0.5f, 8), Vector3(0, 0, 0), Vector3(2, 1, 2));

    // Try loading a glTF model from assets/models/
    {
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::filesystem::path modelDir = std::filesystem::path(exePath).parent_path() / "assets" / "models";

        if (std::filesystem::exists(modelDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(modelDir)) {
                auto ext = entry.path().extension().string();
                if (ext == ".gltf" || ext == ".glb") {
                    m_modelLoaded = ModelLoader::LoadGLTF(
                        entry.path().string(), device, allocator,
                        m_renderContext.GetDirectQueue(), m_renderContext.GetSrvHeap(), m_testModel);
                    if (m_modelLoaded) {
                        SE_LOG_INFO("Loaded test model: {}", entry.path().filename().string());
                        m_scene.AddObject(&m_testModel, "glTF Model", Vector3(0, 0, 0));
                    }
                    break;
                }
            }
        }

        if (!m_modelLoaded) {
            SE_LOG_INFO("No glTF model found in assets/models/ — rendering procedural geometry only");
        }
    }
}

void Application::Shutdown() {
    m_renderContext.GetDirectQueue().Flush();

    if (m_modelLoaded) {
        m_testModel.Shutdown(m_renderContext.GetSrvHeap());
        m_modelLoaded = false;
    }

    // Shutdown procedural model buffers
    for (auto& mesh : m_gridModel.meshes)
        for (auto& prim : mesh.primitives) {
            prim.vertexBuffer.Shutdown();
            prim.indexBuffer.Shutdown();
        }
    for (auto& mesh : m_cubeModel.meshes)
        for (auto& prim : mesh.primitives) {
            prim.vertexBuffer.Shutdown();
            prim.indexBuffer.Shutdown();
        }
    m_gridModel.meshes.clear();
    m_cubeModel.meshes.clear();

    m_scene.Clear();
    m_sceneRenderer.Shutdown();
    m_viewport.Shutdown(m_renderContext.GetSrvHeap());
    m_imguiLayer.Shutdown();
    m_renderContext.Shutdown();
    m_window.Shutdown();
    SE_LOG_INFO("Application shutdown");
}

int Application::Run() {
    while (m_window.ProcessMessages()) {
        m_timer.Tick();
        m_input.Update(m_window.GetHandle());

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
        m_cameraController.Update(m_camera, m_input, dt);
        m_editorController.Update(m_input, m_scene, m_sceneRenderer, m_camera, &m_viewport);

        // Begin render frame
        m_renderContext.BeginFrame();
        auto* cmdList = m_renderContext.GetCommandList().Get();

        // Set SRV heap for rendering
        ID3D12DescriptorHeap* heaps[] = {m_renderContext.GetSrvHeap().GetHeap()};
        cmdList->SetDescriptorHeaps(1, heaps);

        // Phase 1: Render scene to off-screen viewport render target
        m_viewport.BeginRender(m_renderContext.GetCommandList());
        m_sceneRenderer.Render(m_renderContext, m_camera, m_scene, m_editorController.GetSelectedObjectId());
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

                ImGuiID dockRight;
                ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Right, 0.20f, &dockRight, &dockCenter);
                ImGuiID dockRightTop, dockRightBottom;
                ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.5f, &dockRightBottom, &dockRightTop);

                ImGui::DockBuilderDockWindow("Console", dockBottom);
                ImGui::DockBuilderDockWindow("Viewport", dockCenter);
                ImGui::DockBuilderDockWindow("Scene Hierarchy", dockRightTop);
                ImGui::DockBuilderDockWindow("Inspector", dockRightBottom);
                ImGui::DockBuilderFinish(dockspaceId);
            }
        }

        // Create full-screen DockSpace
        ImGui::DockSpaceOverViewport(dockspaceId, ImGui::GetMainViewport());

        m_viewport.OnImGui(m_timer.FPS(), m_timer.DeltaTime());
        m_editorController.RenderUI(m_scene, m_camera, m_cameraController, &m_viewport);
        m_console.Render();
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
