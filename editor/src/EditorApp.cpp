#include <showcase/editor/EditorApp.h>

#include <showcase/core/Log.h>
#include <showcase/core/Platform.h>

#include <imgui_internal.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace {

std::string GetEditorConfigPath() {
    return showcase::GetExecutableDir() + "editor_config.json";
}

} // anonymous namespace

namespace showcase {

// ── Init ──────────────────────────────────────────────────────────

bool EditorApp::Init(const EditorAppDesc &desc) {
    Log::Init();

    if (!m_window.Init(desc.window))
        return false;

    if (!m_renderContext.Init(m_window.GetHandle(), m_window.GetWidth(), m_window.GetHeight())) {
        return false;
    }

    if (!m_console.Init()) {
        SE_LOG_ERROR("Failed to initialize Console");
        return false;
    }

    m_console.RegisterCommand("fps", [this](const std::string &) -> std::string {
        m_viewport.ToggleShowFPS();
        return m_viewport.GetShowFPS() ? "FPS overlay enabled" : "FPS overlay disabled";
    });

    m_console.RegisterCommand("vsync", [this](const std::string &) -> std::string {
        bool enabled = !m_renderContext.GetVSyncEnabled();
        m_renderContext.SetVSyncEnabled(enabled);
        return enabled ? "V-Sync enabled" : "V-Sync disabled";
    });

    if (!m_imguiLayer.Init(m_window.GetHandle(), m_renderContext))
        return false;

    if (!m_viewport.Init(m_renderContext, m_window.GetWidth(), m_window.GetHeight())) {
        return false;
    }

    m_viewport.SetResizeCallback(
        [this](uint32_t w, uint32_t h) { m_sceneRenderer.OnResize(w, h); });

    m_viewport.SetToolbarCallback([this]() { m_editorController.RenderToolbar(m_viewport); });

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

    // Initialize viewport camera (defaults, overridden by config if available)
    m_viewport.InitCamera(Vector3(0.0f, 5.0f, -15.0f), Vector3(0.0f, 0.0f, 0.0f), kPiOver4, 0.1f,
                          1000.0f);

    LoadEditorConfig();

    m_timer.Reset();

    SE_LOG_INFO("Editor initialized");
    return true;
}

// ── Scene setup ──────────────────────────────────────────────────

void EditorApp::BuildDefaultScene() {
    m_sceneRenderer.CreateGridModel(m_renderContext, m_gridModel);
    m_sceneRenderer.CreateCubeModel(m_renderContext, m_cubeModel);

    m_scene.Clear();
    m_scene.AddObject(&m_gridModel, "Ground Grid", Vector3(0, 0, 0));
    m_scene.AddObject(&m_cubeModel, "Cube 1", Vector3(0, 1, 0), Vector3(0, 0, 0), Vector3(2, 2, 2));
    m_scene.AddObject(&m_cubeModel, "Cube 2", Vector3(5, 0.75f, 3), Vector3(0, 0, 0),
                      Vector3(1.5f, 1.5f, 1.5f));
    m_scene.AddObject(&m_cubeModel, "Cube 3", Vector3(-6, 2, -4), Vector3(0, 0, 0),
                      Vector3(3, 4, 3));
    m_scene.AddObject(&m_cubeModel, "Cube 4", Vector3(8, 0.5f, -7), Vector3(0, 0, 0),
                      Vector3(1, 1, 1));
    m_scene.AddObject(&m_cubeModel, "Cube 5", Vector3(-3, 0.5f, 8), Vector3(0, 0, 0),
                      Vector3(2, 1, 2));

    // Try loading a glTF model from assets/models/
    {
        std::filesystem::path modelDir =
            std::filesystem::path(GetExecutableDir()) / "assets" / "models";

        if (std::filesystem::exists(modelDir)) {
            for (const auto &entry : std::filesystem::directory_iterator(modelDir)) {
                std::string ext = entry.path().extension().string();
                if (ext == ".gltf" || ext == ".glb") {
                    m_modelLoaded =
                        ModelLoader::LoadGLTF(m_renderContext, entry.path().string(), m_testModel);
                    if (m_modelLoaded) {
                        SE_LOG_INFO("Loaded test model: {}", entry.path().filename().string());
                        m_scene.AddObject(&m_testModel, "glTF Model", Vector3(0, 0, 0));
                    }
                    break;
                }
            }
        }

        if (!m_modelLoaded) {
            SE_LOG_INFO(
                "No glTF model found in assets/models/ — rendering procedural geometry only");
        }
    }
}

// ── Config persistence ───────────────────────────────────────────

void EditorApp::SaveEditorConfig() {
    nlohmann::json j;

    // Camera
    const Vector3& pos = m_viewport.GetCamera().GetPosition();
    j["camera"]["position"] = {pos.x, pos.y, pos.z};
    j["camera"]["yaw"] = m_viewport.GetYaw();
    j["camera"]["pitch"] = m_viewport.GetPitch();

    // Viewport settings
    j["viewport"]["showFPS"] = m_viewport.GetShowFPS();
    j["viewport"]["cameraMoveSpeed"] = m_viewport.cameraMoveSpeed;
    j["viewport"]["cameraLookSpeed"] = m_viewport.cameraLookSpeed;

    // Render settings
    j["render"]["vsync"] = m_renderContext.GetVSyncEnabled();

    // Gizmo settings
    j["gizmo"]["operation"] = static_cast<int>(m_editorController.GetGizmoOperation());
    j["gizmo"]["mode"] = static_cast<int>(m_editorController.GetGizmoMode());
    j["gizmo"]["useSnap"] = m_editorController.GetUseSnap();
    j["gizmo"]["snapTranslate"] = m_editorController.GetSnapTranslate();
    j["gizmo"]["snapRotate"] = m_editorController.GetSnapRotate();
    j["gizmo"]["snapScale"] = m_editorController.GetSnapScale();

    // Console settings
    j["console"]["levelFilter"] = m_console.GetLevelFilter();
    j["console"]["autoScroll"] = m_console.GetAutoScroll();

    std::ofstream file(GetEditorConfigPath());
    if (file.is_open()) {
        file << j.dump(2);
        SE_LOG_INFO("Editor config saved");
    }
}

void EditorApp::LoadEditorConfig() {
    std::ifstream file(GetEditorConfigPath());
    if (!file.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    if (!nlohmann::json::accept(content)) {
        SE_LOG_WARN("Failed to parse editor config");
        return;
    }
    nlohmann::json j = nlohmann::json::parse(content);

    try {
        // Camera
        if (j.contains("camera")) {
            auto& cam = j["camera"];
            if (cam.contains("position") && cam["position"].is_array() &&
                cam["position"].size() >= 3 && cam.contains("yaw") && cam.contains("pitch")) {
                Vector3 pos(cam["position"][0].get<float>(),
                            cam["position"][1].get<float>(),
                            cam["position"][2].get<float>());
                float yaw = cam["yaw"].get<float>();
                float pitch = cam["pitch"].get<float>();
                m_viewport.InitCamera(pos, yaw, pitch, kPiOver4, 0.1f, 1000.0f);
            }
        }

        // Viewport settings
        if (j.contains("viewport")) {
            auto& vp = j["viewport"];
            if (vp.contains("showFPS")) m_viewport.SetShowFPS(vp["showFPS"].get<bool>());
            if (vp.contains("cameraMoveSpeed")) m_viewport.cameraMoveSpeed = vp["cameraMoveSpeed"].get<float>();
            if (vp.contains("cameraLookSpeed")) m_viewport.cameraLookSpeed = vp["cameraLookSpeed"].get<float>();
        }

        // Render settings
        if (j.contains("render")) {
            auto& r = j["render"];
            if (r.contains("vsync")) m_renderContext.SetVSyncEnabled(r["vsync"].get<bool>());
        }

        // Gizmo settings
        if (j.contains("gizmo")) {
            auto& g = j["gizmo"];
            if (g.contains("operation")) m_editorController.SetGizmoOperation(
                static_cast<ImGuizmo::OPERATION>(g["operation"].get<int>()));
            if (g.contains("mode")) m_editorController.SetGizmoMode(
                static_cast<ImGuizmo::MODE>(g["mode"].get<int>()));
            if (g.contains("useSnap")) m_editorController.SetUseSnap(g["useSnap"].get<bool>());
            if (g.contains("snapTranslate")) m_editorController.SetSnapTranslate(g["snapTranslate"].get<float>());
            if (g.contains("snapRotate")) m_editorController.SetSnapRotate(g["snapRotate"].get<float>());
            if (g.contains("snapScale")) m_editorController.SetSnapScale(g["snapScale"].get<float>());
        }

        // Console settings
        if (j.contains("console")) {
            auto& c = j["console"];
            if (c.contains("levelFilter")) m_console.SetLevelFilter(c["levelFilter"].get<int>());
            if (c.contains("autoScroll")) m_console.SetAutoScroll(c["autoScroll"].get<bool>());
        }
    } catch (const nlohmann::json::exception& e) {
        SE_LOG_WARN("Failed to restore editor config: {}", e.what());
        return;
    }

    SE_LOG_INFO("Editor config restored");
}

// ── Shutdown ─────────────────────────────────────────────────────

void EditorApp::Shutdown() {
    SaveEditorConfig();

    m_renderContext.GetDirectQueue().Flush();

    if (m_modelLoaded) {
        m_testModel.Shutdown(m_renderContext);
        m_modelLoaded = false;
    }

    // Shutdown procedural model buffers
    for (auto &mesh : m_gridModel.meshes) {
        for (auto &prim : mesh.primitives) {
            prim.vertexBuffer.Shutdown();
            prim.indexBuffer.Shutdown();
        }
    }
    for (auto &mesh : m_cubeModel.meshes) {
        for (auto &prim : mesh.primitives) {
            prim.vertexBuffer.Shutdown();
            prim.indexBuffer.Shutdown();
        }
    }
    m_gridModel.meshes.clear();
    m_cubeModel.meshes.clear();

    m_scene.Clear();
    m_sceneRenderer.Shutdown();
    m_viewport.Shutdown(m_renderContext);
    m_imguiLayer.Shutdown();
    m_renderContext.Shutdown();
    m_window.Shutdown();
    SE_LOG_INFO("Editor shutdown");
}

// ── Main loop ────────────────────────────────────────────────────

int EditorApp::Run() {
    while (m_window.ProcessMessages()) {
        m_timer.Tick();
        m_input.Update(m_window.GetHandle());

        if (m_window.IsMinimized()) {
            SleepMs(10);
            continue;
        }

        // Handle deferred resize
        if (m_resizePending) {
            m_resizePending = false;
            OnResize(m_pendingWidth, m_pendingHeight);
        }

        float dt = m_timer.DeltaTime();

        // Update
        m_viewport.UpdateCamera(m_input, dt);
        m_editorController.Update(m_input, m_scene, m_sceneRenderer, m_viewport);

        // Begin render frame
        m_renderContext.BeginFrame();

        // Phase 1: Render scene to off-screen viewport render target
        m_viewport.BeginRender(m_renderContext.GetCommandList());
        m_sceneRenderer.Render(m_renderContext, m_viewport.GetCamera(), m_scene,
                               m_editorController.GetSelectedObjectId());
        m_viewport.EndRender(m_renderContext.GetCommandList());

        // Phase 2: Render ImGui to back buffer
        float clearColor[] = {0.05f, 0.05f, 0.08f, 1.0f};
        m_renderContext.BeginBackBufferPass(clearColor);

        m_imguiLayer.BeginFrame();

        // DockBuilder must run BEFORE DockSpaceOverViewport
        ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
        static bool s_dockLayoutChecked = false;
        if (!s_dockLayoutChecked) {
            s_dockLayoutChecked = true;
            ImGuiDockNode *node = ImGui::DockBuilderGetNode(dockspaceId);
            if (!node || !node->IsSplitNode()) {
                ImGui::DockBuilderRemoveNode(dockspaceId);
                ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
                const ImGuiViewport *vp = ImGui::GetMainViewport();
                ImGui::DockBuilderSetNodeSize(dockspaceId, vp->Size);

                ImGuiID dockBottom;
                ImGuiID dockCenter;
                ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Down, 0.25f, &dockBottom,
                                            &dockCenter);

                ImGuiID dockRight;
                ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Right, 0.20f, &dockRight,
                                            &dockCenter);
                ImGuiID dockRightTop, dockRightBottom;
                ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.5f, &dockRightBottom,
                                            &dockRightTop);

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
        m_editorController.RenderUI(m_scene, m_viewport);
        m_console.Render();
        m_imguiLayer.EndFrame(m_renderContext.GetCommandList());

        m_renderContext.EndBackBufferPass();

        // End frame
        m_renderContext.EndFrame();
    }

    return 0;
}

// ── Resize ────────────────────────────────────────────────────────

void EditorApp::OnResize(uint32_t width, uint32_t height) {
    m_renderContext.Resize(width, height);
    SE_LOG_INFO("Editor resized: {}x{}", width, height);
}

} // namespace showcase
