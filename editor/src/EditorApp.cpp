#include <showcase/editor/EditorApp.h>

#include <showcase/core/Log.h>
#include <showcase/core/Platform.h>
#include <showcase/core/JsonDocument.h>

#include <imgui_internal.h>

#include <filesystem>

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
    JsonDocument doc;

    // Camera
    const Vector3& pos = m_viewport.GetCamera().GetPosition();
    auto cam = doc["camera"];
    cam["position"].SetFloatArray({pos.x, pos.y, pos.z});
    cam["yaw"].Set(m_viewport.GetYaw());
    cam["pitch"].Set(m_viewport.GetPitch());

    // Viewport settings
    auto vp = doc["viewport"];
    vp["showFPS"].Set(m_viewport.GetShowFPS());
    vp["cameraMoveSpeed"].Set(m_viewport.cameraMoveSpeed);
    vp["cameraLookSpeed"].Set(m_viewport.cameraLookSpeed);

    // Render settings
    doc["render"]["vsync"].Set(m_renderContext.GetVSyncEnabled());

    // Gizmo settings
    auto g = doc["gizmo"];
    g["operation"].Set(static_cast<int>(m_editorController.GetGizmoOperation()));
    g["mode"].Set(static_cast<int>(m_editorController.GetGizmoMode()));
    g["useSnap"].Set(m_editorController.GetUseSnap());
    g["snapTranslate"].Set(m_editorController.GetSnapTranslate());
    g["snapRotate"].Set(m_editorController.GetSnapRotate());
    g["snapScale"].Set(m_editorController.GetSnapScale());

    // Console settings
    auto con = doc["console"];
    con["levelFilter"].Set(m_console.GetLevelFilter());
    con["autoScroll"].Set(m_console.GetAutoScroll());

    if (doc.SaveToFile(GetEditorConfigPath())) {
        SE_LOG_INFO("Editor config saved");
    }
}

void EditorApp::LoadEditorConfig() {
    JsonDocument doc;
    if (!doc.LoadFromFile(GetEditorConfigPath())) {
        return;
    }

    // Camera
    auto cam = doc["camera"];
    auto pos = cam["position"];
    if (pos.IsArray() && pos.Size() >= 3 && cam.Contains("yaw") && cam.Contains("pitch")) {
        Vector3 cameraPos(pos[0].GetFloat(), pos[1].GetFloat(), pos[2].GetFloat());
        m_viewport.InitCamera(cameraPos, cam["yaw"].GetFloat(), cam["pitch"].GetFloat(),
                              kPiOver4, 0.1f, 1000.0f);
    }

    // Viewport settings
    auto vp = doc["viewport"];
    if (vp.Contains("showFPS")) {
        m_viewport.SetShowFPS(vp["showFPS"].GetBool());
    }
    if (vp.Contains("cameraMoveSpeed")) {
        m_viewport.cameraMoveSpeed = vp["cameraMoveSpeed"].GetFloat();
    }
    if (vp.Contains("cameraLookSpeed")) {
        m_viewport.cameraLookSpeed = vp["cameraLookSpeed"].GetFloat();
    }

    // Render settings
    auto r = doc["render"];
    if (r.Contains("vsync")) {
        m_renderContext.SetVSyncEnabled(r["vsync"].GetBool());
    }

    // Gizmo settings
    auto g = doc["gizmo"];
    if (g.Contains("operation")) {
        m_editorController.SetGizmoOperation(
            static_cast<ImGuizmo::OPERATION>(g["operation"].GetInt()));
    }
    if (g.Contains("mode")) {
        m_editorController.SetGizmoMode(
            static_cast<ImGuizmo::MODE>(g["mode"].GetInt()));
    }
    if (g.Contains("useSnap")) {
        m_editorController.SetUseSnap(g["useSnap"].GetBool());
    }
    if (g.Contains("snapTranslate")) {
        m_editorController.SetSnapTranslate(g["snapTranslate"].GetFloat());
    }
    if (g.Contains("snapRotate")) {
        m_editorController.SetSnapRotate(g["snapRotate"].GetFloat());
    }
    if (g.Contains("snapScale")) {
        m_editorController.SetSnapScale(g["snapScale"].GetFloat());
    }

    // Console settings
    auto con = doc["console"];
    if (con.Contains("levelFilter")) {
        m_console.SetLevelFilter(con["levelFilter"].GetInt());
    }
    if (con.Contains("autoScroll")) {
        m_console.SetAutoScroll(con["autoScroll"].GetBool());
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
