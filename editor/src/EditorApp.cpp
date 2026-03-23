#include <showcase/editor/EditorApp.h>

#include <showcase/core/JsonDocument.h>
#include <showcase/core/Log.h>
#include <showcase/core/Platform.h>

#include <imgui_internal.h>

#include <filesystem>

namespace {

std::string GetEditorConfigPath() {
    return showcase::GetExecutableDir() + "editor_config.json";
}

const char* kSceneFileFilter = "Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
const char* kSceneFileExt = "scene";

} // anonymous namespace

namespace showcase {

// ── Init ──────────────────────────────────────────────────────────

bool EditorApp::Init(const EditorAppDesc& desc) {
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

    m_console.RegisterCommand("fps", [this](const std::string&) -> std::string {
        m_viewport.ToggleShowFPS();
        return m_viewport.GetShowFPS() ? "FPS overlay enabled" : "FPS overlay disabled";
    });

    m_console.RegisterCommand("vsync", [this](const std::string&) -> std::string {
        bool enabled = !m_renderContext.GetVSyncEnabled();
        m_renderContext.SetVSyncEnabled(enabled);
        return enabled ? "V-Sync enabled" : "V-Sync disabled";
    });

    if (!m_imguiLayer.Init(m_window.GetHandle(), m_renderContext))
        return false;

    if (!m_viewport.Init(m_renderContext, m_window.GetWidth(), m_window.GetHeight())) {
        return false;
    }

    m_viewport.SetResizeCallback([this](uint32_t w, uint32_t h) { m_sceneRenderer.OnResize(w, h); });

    m_viewport.SetToolbarCallback([this]() { m_editorController.RenderToolbar(m_viewport, m_sceneRenderer); });

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
    m_viewport.InitCamera(Vector3(0.0f, 5.0f, -15.0f), Vector3(0.0f, 0.0f, 0.0f), kPiOver4, 0.1f, 1000.0f);

    LoadEditorConfig();

    m_editorController.SetDirtyCallback([this]() {
        m_sceneDirty = true;
        UpdateWindowTitle();
    });

    m_editorController.SetAddObjectCallback([this](const std::string& modelSource) -> SceneObject* {
        Model* model = ResolveModel(modelSource);
        if (!model)
            return nullptr;
        SceneObject& obj = m_scene.AddObject(model, "Cube", Vector3(0, 1, 0));
        obj.modelSource = modelSource;
        return &obj;
    });

    UpdateWindowTitle();

    m_timer.Reset();

    SE_LOG_INFO("Editor initialized");
    return true;
}

// ── Scene setup ──────────────────────────────────────────────────

void EditorApp::BuildDefaultScene() {
    m_sceneRenderer.CreateGridModel(m_renderContext, m_gridModel);
    m_sceneRenderer.CreateCubeModel(m_renderContext, m_cubeModel);

    m_scene.Clear();
    m_scene.AddObject(&m_gridModel, "Ground Grid", Vector3(0, 0, 0)).modelSource = "builtin:grid";
    m_scene.AddObject(&m_cubeModel, "Cube 1", Vector3(0, 1, 0), Vector3(0, 0, 0), Vector3(2, 2, 2)).modelSource =
        "builtin:cube";
    m_scene.AddObject(&m_cubeModel, "Cube 2", Vector3(5, 0.75f, 3), Vector3(0, 0, 0), Vector3(1.5f, 1.5f, 1.5f))
        .modelSource = "builtin:cube";
    m_scene.AddObject(&m_cubeModel, "Cube 3", Vector3(-6, 2, -4), Vector3(0, 0, 0), Vector3(3, 4, 3)).modelSource =
        "builtin:cube";
    m_scene.AddObject(&m_cubeModel, "Cube 4", Vector3(8, 0.5f, -7), Vector3(0, 0, 0), Vector3(1, 1, 1)).modelSource =
        "builtin:cube";
    m_scene.AddObject(&m_cubeModel, "Cube 5", Vector3(-3, 0.5f, 8), Vector3(0, 0, 0), Vector3(2, 1, 2)).modelSource =
        "builtin:cube";

    // Try loading a glTF model from assets/models/
    {
        std::filesystem::path modelDir = std::filesystem::path(GetExecutableDir()) / "assets" / "models";

        if (std::filesystem::exists(modelDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(modelDir)) {
                std::string ext = entry.path().extension().string();
                if (ext == ".gltf" || ext == ".glb") {
                    m_modelLoaded = ModelLoader::LoadGLTF(m_renderContext, entry.path().string(), m_testModel);
                    if (m_modelLoaded) {
                        SE_LOG_INFO("Loaded test model: {}", entry.path().filename().string());
                        std::string relPath = "assets/models/" + entry.path().filename().string();
                        m_testModelSource = "file:" + relPath;
                        m_scene.AddObject(&m_testModel, "glTF Model", Vector3(0, 0, 0)).modelSource = m_testModelSource;
                    }
                    break;
                }
            }
        }

        if (!m_modelLoaded) {
            SE_LOG_INFO("No glTF model found in assets/models/ — rendering procedural geometry only");
        }
    }

    BuildModelRegistry();
}

// ── Config persistence ───────────────────────────────────────────

void EditorApp::SaveEditorConfig() {
    JsonDocument doc;

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

    // Grid settings
    auto grid = doc["grid"];
    grid["visible"].Set(m_sceneRenderer.GetGridSettings().visible);
    grid["opacity"].Set(m_sceneRenderer.GetGridSettings().opacity);
    grid["fadeStart"].Set(m_sceneRenderer.GetGridSettings().fadeStart);
    grid["fadeEnd"].Set(m_sceneRenderer.GetGridSettings().fadeEnd);

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
        m_editorController.SetGizmoOperation(static_cast<ImGuizmo::OPERATION>(g["operation"].GetInt()));
    }
    if (g.Contains("mode")) {
        m_editorController.SetGizmoMode(static_cast<ImGuizmo::MODE>(g["mode"].GetInt()));
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

    // Grid settings
    auto grid = doc["grid"];
    auto& gs = m_sceneRenderer.GetGridSettings();
    if (grid.Contains("visible"))
        gs.visible = grid["visible"].GetBool();
    if (grid.Contains("opacity"))
        gs.opacity = grid["opacity"].GetFloat();
    if (grid.Contains("fadeStart"))
        gs.fadeStart = grid["fadeStart"].GetFloat();
    if (grid.Contains("fadeEnd"))
        gs.fadeEnd = grid["fadeEnd"].GetFloat();

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

// ── Scene document management ────────────────────────────────────

void EditorApp::BuildModelRegistry() {
    m_modelRegistry.clear();
    m_modelRegistry["builtin:grid"] = &m_gridModel;
    m_modelRegistry["builtin:cube"] = &m_cubeModel;
    if (m_modelLoaded && !m_testModelSource.empty()) {
        m_modelRegistry[m_testModelSource] = &m_testModel;
    }
}

Model* EditorApp::ResolveModel(const std::string& modelSource) {
    auto it = m_modelRegistry.find(modelSource);
    if (it != m_modelRegistry.end()) {
        return it->second;
    }

    // Try loading file-based models (file:relative/path.gltf)
    if (modelSource.rfind("file:", 0) == 0) {
        std::string relPath = modelSource.substr(5);
        std::string absPath = GetExecutableDir() + relPath;

        auto model = std::make_unique<Model>();
        if (ModelLoader::LoadGLTF(m_renderContext, absPath, *model)) {
            SE_LOG_INFO("Dynamically loaded model: {}", relPath);
            Model* ptr = model.get();
            m_dynamicModels.push_back(std::move(model));
            m_modelRegistry[modelSource] = ptr;
            return ptr;
        }

        SE_LOG_WARN("Failed to load model: {}", absPath);
    }

    return nullptr;
}

void EditorApp::ResolveSceneModels() {
    for (auto& obj : m_scene.GetObjects()) {
        obj.model = ResolveModel(obj.modelSource);
        obj.RecomputeWorldTransform();
        obj.UpdateAABB();
    }
}

void EditorApp::NewScene() {
    if (m_sceneDirty) {
        DialogResult result =
            ShowConfirmDialog(m_window.GetHandle(), "ShowcaseEditor", "Save changes to current scene?");
        if (result == DialogResult::Cancel)
            return;
        if (result == DialogResult::Yes && !SaveScene())
            return;
    }

    // Flush GPU and clean up all models before rebuilding
    m_renderContext.GetDirectQueue().Flush();
    for (auto& model : m_dynamicModels) {
        model->Shutdown(m_renderContext);
    }
    m_dynamicModels.clear();
    m_modelRegistry.clear();

    if (m_modelLoaded) {
        m_testModel.Shutdown(m_renderContext);
        m_modelLoaded = false;
        m_testModelSource.clear();
    }
    for (auto& mesh : m_gridModel.meshes) {
        for (auto& prim : mesh.primitives) {
            prim.vertexBuffer.Shutdown();
            prim.indexBuffer.Shutdown();
        }
    }
    m_gridModel.meshes.clear();
    for (auto& mesh : m_cubeModel.meshes) {
        for (auto& prim : mesh.primitives) {
            prim.vertexBuffer.Shutdown();
            prim.indexBuffer.Shutdown();
        }
    }
    m_cubeModel.meshes.clear();

    m_editorController.ClearSelection();
    BuildDefaultScene();

    // Reset camera to default
    m_viewport.InitCamera(Vector3(0.0f, 5.0f, -15.0f), Vector3(0.0f, 0.0f, 0.0f), kPiOver4, 0.1f, 1000.0f);

    m_currentScenePath.clear();
    m_sceneDirty = false;
    UpdateWindowTitle();
}

void EditorApp::OpenScene() {
    if (m_sceneDirty) {
        DialogResult result =
            ShowConfirmDialog(m_window.GetHandle(), "ShowcaseEditor", "Save changes to current scene?");
        if (result == DialogResult::Cancel)
            return;
        if (result == DialogResult::Yes && !SaveScene())
            return;
    }

    std::string path = OpenFileDialog(m_window.GetHandle(), kSceneFileFilter, kSceneFileExt);
    if (path.empty())
        return;

    // Flush GPU and clean up dynamic models
    m_renderContext.GetDirectQueue().Flush();
    for (auto& model : m_dynamicModels) {
        model->Shutdown(m_renderContext);
    }
    m_dynamicModels.clear();
    m_modelRegistry.clear();

    m_editorController.ClearSelection();

    JsonDocument doc;
    if (!doc.LoadFromFile(path)) {
        ShowErrorMessage(m_window.GetHandle(), "ShowcaseEditor", "Failed to load scene file.");
        return;
    }
    if (!m_scene.Deserialize(doc)) {
        ShowErrorMessage(m_window.GetHandle(), "ShowcaseEditor", "Scene file missing 'objects' array.");
        return;
    }

    // Restore editor camera from scene
    auto cam = doc["camera"];
    auto camPos = cam["position"];
    if (camPos.IsArray() && camPos.Size() >= 3 && cam.Contains("yaw") && cam.Contains("pitch")) {
        m_viewport.InitCamera(Vector3(camPos[0].GetFloat(), camPos[1].GetFloat(), camPos[2].GetFloat()),
                              cam["yaw"].GetFloat(), cam["pitch"].GetFloat(), kPiOver4, 0.1f, 1000.0f);
    }

    // Rebuild registry with builtin models
    BuildModelRegistry();
    ResolveSceneModels();

    m_currentScenePath = path;
    m_sceneDirty = false;
    UpdateWindowTitle();
}

bool EditorApp::SaveScene() {
    if (m_currentScenePath.empty()) {
        return SaveSceneAs();
    }

    JsonDocument doc;
    m_scene.Serialize(doc);

    // Editor camera
    const Vector3& camPos = m_viewport.GetCamera().GetPosition();
    auto cam = doc["camera"];
    cam["position"].SetFloatArray({camPos.x, camPos.y, camPos.z});
    cam["yaw"].Set(m_viewport.GetYaw());
    cam["pitch"].Set(m_viewport.GetPitch());

    if (!doc.SaveToFile(m_currentScenePath)) {
        SE_LOG_ERROR("Failed to save scene: {}", m_currentScenePath);
        return false;
    }

    SE_LOG_INFO("Scene saved: {}", m_currentScenePath);
    m_sceneDirty = false;
    UpdateWindowTitle();
    return true;
}

bool EditorApp::SaveSceneAs() {
    std::string path = SaveFileDialog(m_window.GetHandle(), kSceneFileFilter, kSceneFileExt);
    if (path.empty())
        return false;

    m_currentScenePath = path;
    return SaveScene();
}

void EditorApp::UpdateWindowTitle() {
    std::string title = "ShowcaseEditor - ";
    if (m_currentScenePath.empty()) {
        title += "Untitled";
    } else {
        std::filesystem::path p(m_currentScenePath);
        title += p.filename().string();
    }
    if (m_sceneDirty) {
        title += "*";
    }
    m_window.SetTitle(title.c_str());
}

void EditorApp::RenderMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                m_pendingAction = PendingAction::NewScene;
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                m_pendingAction = PendingAction::OpenScene;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                SaveScene();
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                SaveSceneAs();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

// ── Shutdown ─────────────────────────────────────────────────────

void EditorApp::Shutdown() {
    SaveEditorConfig();

    m_renderContext.GetDirectQueue().Flush();

    // Shutdown dynamic models loaded via OpenScene
    for (auto& model : m_dynamicModels) {
        model->Shutdown(m_renderContext);
    }
    m_dynamicModels.clear();

    if (m_modelLoaded) {
        m_testModel.Shutdown(m_renderContext);
        m_modelLoaded = false;
    }

    // Shutdown procedural model buffers
    for (auto& mesh : m_gridModel.meshes) {
        for (auto& prim : mesh.primitives) {
            prim.vertexBuffer.Shutdown();
            prim.indexBuffer.Shutdown();
        }
    }
    for (auto& mesh : m_cubeModel.meshes) {
        for (auto& prim : mesh.primitives) {
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

        // Keyboard shortcuts (Ctrl+key) — New/Open use deferred actions to avoid GPU ops mid-frame
        if (m_input.IsKeyDown(Key::kControl)) {
            if (m_input.IsKeyDown(Key::kShift) && m_input.IsKeyPressed('S')) {
                SaveSceneAs();
            } else if (m_input.IsKeyPressed('S')) {
                SaveScene();
            } else if (m_input.IsKeyPressed('O')) {
                m_pendingAction = PendingAction::OpenScene;
            } else if (m_input.IsKeyPressed('N')) {
                m_pendingAction = PendingAction::NewScene;
            }
        }

        // Process deferred menu actions before rendering (GPU resource changes can't happen
        // mid-frame)
        if (m_pendingAction == PendingAction::NewScene) {
            m_pendingAction = PendingAction::None;
            NewScene();
        } else if (m_pendingAction == PendingAction::OpenScene) {
            m_pendingAction = PendingAction::None;
            OpenScene();
        }

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
        RenderMainMenuBar();

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
