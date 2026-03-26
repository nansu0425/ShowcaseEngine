#include <showcase/editor/EditorApp.h>

#include <showcase/core/FileSystem.h>
#include <showcase/core/JsonDocument.h>
#include <showcase/core/Log.h>
#include <showcase/core/Profiler.h>
#include <showcase/editor/NativeDialog.h>

#include <imgui_internal.h>

#include <shellapi.h>

#include <algorithm>
#include <filesystem>

namespace {

std::string GetEditorConfigPath() {
    return showcase::GetExecutableDir() + "editor_config.json";
}

const char* kSceneFileFilter = "Scene Files (*.scene)\0*.scene\0All Files (*.*)\0*.*\0";
const char* kSceneFileExt = "scene";

bool LaunchProcess(const std::string& exePath) {
    HINSTANCE result = ShellExecuteA(nullptr, "open", exePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
}

} // anonymous namespace

namespace showcase {

// ── Init ──────────────────────────────────────────────────────────

bool EditorApp::Init(const EditorAppDesc& desc) {
    SE_ZONE_SCOPED;
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
    m_viewport.InitCamera(Vector3(0.0f, 5.0f, -15.0f), Vector3(0.0f, 0.0f, 0.0f), kPiOver4, 0.1f, 1000.0f);

    LoadEditorConfig();

    m_editorController.SetDirtyCallback([this]() {
        m_sceneDirty = true;
        UpdateWindowTitle();
    });

    m_editorController.SetAddObjectCallback([this](const std::string& modelSource) -> SceneObject* {
        if (modelSource.empty()) {
            SceneObject& obj = m_scene.AddObject({nullptr, "Empty", Vector3(0, 1, 0)});
            return &obj;
        }
        Model* model = m_assetManager.LoadModel(modelSource);
        if (!model)
            return nullptr;
        SceneObject& obj = m_scene.AddObject({model, "Cube", Vector3(0, 1, 0)});
        obj.modelComp->modelSource = modelSource;
        return &obj;
    });

    m_editorController.SetAssetListCallback([this]() -> std::vector<std::string> {
        std::vector<std::string> sources = m_assetManager.GetAvailableSources();
        for (const auto& asset : m_availableAssets)
            sources.push_back("file:" + asset.relativePath);
        std::sort(sources.begin(), sources.end());
        sources.erase(std::unique(sources.begin(), sources.end()), sources.end());
        return sources;
    });

    m_editorController.SetResolveModelCallback(
        [this](const std::string& src) -> Model* { return m_assetManager.LoadModel(src); });

    m_commandHistory.SetDirtyCallback([this]() {
        m_sceneDirty = true;
        UpdateWindowTitle();
    });
    m_editorController.SetCommandHistory(&m_commandHistory);

    ScanAssets();

    UpdateWindowTitle();

    m_timer.Reset();

    SE_LOG_INFO("Editor initialized");
    return true;
}

// ── Scene setup ──────────────────────────────────────────────────

void EditorApp::BuildDefaultScene() {
    m_assetManager.Init(m_renderContext, m_sceneRenderer);
    m_scene.Clear();
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

void EditorApp::ResolveSceneModels() {
    SE_ZONE_SCOPED_C(profile::kColorAssetIO);
    for (auto& obj : m_scene.GetObjects()) {
        if (obj.modelComp.has_value()) {
            obj.modelComp->model = m_assetManager.LoadModel(obj.modelComp->modelSource);
        }
        obj.RecomputeWorldTransform();
        obj.UpdateAABB();
    }
}

void EditorApp::ScanAssets() {
    SE_ZONE_SCOPED_C(profile::kColorAssetIO);
    namespace fs = std::filesystem;

    m_availableAssets.clear();
    std::string assetsPath = GetExecutableDir() + "assets/";

    std::error_code ec;
    if (!fs::exists(assetsPath, ec))
        return;

    for (const auto& entry : fs::recursive_directory_iterator(assetsPath, ec)) {
        if (!entry.is_regular_file())
            continue;

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (ext != ".glb" && ext != ".gltf")
            continue;

        fs::path relPath = fs::relative(entry.path(), GetExecutableDir(), ec);
        if (ec)
            continue;

        AssetEntry asset;
        asset.relativePath = relPath.generic_string();
        asset.filename = entry.path().filename().string();
        asset.extension = ext;
        m_availableAssets.push_back(std::move(asset));
    }

    std::sort(m_availableAssets.begin(), m_availableAssets.end(),
              [](const AssetEntry& a, const AssetEntry& b) { return a.filename < b.filename; });
}

void EditorApp::ImportAsset() {
    SE_ZONE_SCOPED_C(profile::kColorAssetIO);
    const char* filter = "glTF Files (*.glb;*.gltf)\0*.glb;*.gltf\0All Files (*.*)\0*.*\0";
    std::string srcPath = OpenFileDialog(m_window.GetHandle(), filter, "glb");
    if (srcPath.empty())
        return;

    SE_ZONE_TEXT(srcPath.c_str(), srcPath.size());

    namespace fs = std::filesystem;

    fs::path source(srcPath);
    std::string filename = source.filename().string();

    std::string assetsPath = GetExecutableDir() + "assets/";
    fs::path destDir = fs::path(assetsPath) / "models";
    std::error_code ec;
    fs::create_directories(destDir, ec);

    fs::path destPath = destDir / filename;

    if (fs::exists(destPath)) {
        DialogResult result = ShowConfirmDialog(m_window.GetHandle(), "Overwrite File",
                                                ("File \"" + filename + "\" already exists. Overwrite?").c_str());
        if (result != DialogResult::Yes)
            return;
    }

    fs::copy_file(source, destPath, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        SE_LOG_ERROR("Failed to import asset: {}", ec.message());
        return;
    }

    SE_LOG_INFO("Imported asset: {}", filename);
    ScanAssets();
}

void EditorApp::NewScene() {
    SE_ZONE_SCOPED_C(profile::kColorAssetIO);
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
    m_assetManager.Shutdown();

    m_editorController.ClearSelection();
    m_commandHistory.Clear();
    BuildDefaultScene();

    // Reset camera to default
    m_viewport.InitCamera(Vector3(0.0f, 5.0f, -15.0f), Vector3(0.0f, 0.0f, 0.0f), kPiOver4, 0.1f, 1000.0f);

    m_currentScenePath.clear();
    m_sceneDirty = false;
    UpdateWindowTitle();
    SE_MESSAGE_L("New scene created");
}

void EditorApp::OpenScene() {
    SE_ZONE_SCOPED_C(profile::kColorAssetIO);
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

    SE_ZONE_TEXT(path.c_str(), path.size());

    // Flush GPU and clean up models
    m_renderContext.GetDirectQueue().Flush();
    m_assetManager.Shutdown();
    m_assetManager.Init(m_renderContext, m_sceneRenderer);

    m_editorController.ClearSelection();
    m_commandHistory.Clear();

    JsonDocument doc;
    if (!doc.LoadFromFile(path)) {
        MessageBoxA(static_cast<HWND>(m_window.GetHandle()), "Failed to load scene file.", "ShowcaseEditor",
                    MB_OK | MB_ICONERROR);
        return;
    }
    if (!m_scene.Deserialize(doc)) {
        MessageBoxA(static_cast<HWND>(m_window.GetHandle()), "Scene file missing 'objects' array.", "ShowcaseEditor",
                    MB_OK | MB_ICONERROR);
        return;
    }

    // Restore editor camera from scene
    auto cam = doc["camera"];
    auto camPos = cam["position"];
    if (camPos.IsArray() && camPos.Size() >= 3 && cam.Contains("yaw") && cam.Contains("pitch")) {
        m_viewport.InitCamera(Vector3(camPos[0].GetFloat(), camPos[1].GetFloat(), camPos[2].GetFloat()),
                              cam["yaw"].GetFloat(), cam["pitch"].GetFloat(), kPiOver4, 0.1f, 1000.0f);
    }

    ResolveSceneModels();

    m_currentScenePath = path;
    m_sceneDirty = false;
    UpdateWindowTitle();
    SE_MESSAGE_L("Scene loaded");
}

bool EditorApp::SaveScene() {
    SE_ZONE_SCOPED_C(profile::kColorAssetIO);
    if (m_currentScenePath.empty()) {
        return SaveSceneAs();
    }

    SE_ZONE_TEXT(m_currentScenePath.c_str(), m_currentScenePath.size());

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
    SE_MESSAGE_L("Scene saved");
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
            if (ImGui::MenuItem("New Scene")) {
                m_pendingAction = PendingAction::NewScene;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Ctrl+N");
            if (ImGui::MenuItem("Open...")) {
                m_pendingAction = PendingAction::OpenScene;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Ctrl+O");
            ImGui::Separator();
            if (ImGui::MenuItem("Save")) {
                SaveScene();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Ctrl+S");
            if (ImGui::MenuItem("Save As...")) {
                SaveSceneAs();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Ctrl+Shift+S");
            ImGui::Separator();
            if (ImGui::MenuItem("Import Asset...")) {
                ImportAsset();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            std::string undoLabel = m_commandHistory.CanUndo() ? "Undo " + m_commandHistory.GetUndoName() : "Undo";
            if (ImGui::MenuItem(undoLabel.c_str(), nullptr, false, m_commandHistory.CanUndo())) {
                m_commandHistory.Undo();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Ctrl+Z");
            std::string redoLabel = m_commandHistory.CanRedo() ? "Redo " + m_commandHistory.GetRedoName() : "Redo";
            if (ImGui::MenuItem(redoLabel.c_str(), nullptr, false, m_commandHistory.CanRedo())) {
                m_commandHistory.Redo();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Ctrl+Y / Ctrl+Shift+Z");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools")) {
#ifdef SHOWCASE_TRACY_ENABLED
            if (ImGui::MenuItem("Tracy Profiler")) {
                namespace fs = std::filesystem;
                fs::path tracyPath = fs::path(GetExecutableDir()) / ".." / ".." / ".." / "tools" / "tracy-profiler.exe";
                std::error_code ec;
                tracyPath = fs::canonical(tracyPath, ec);
                if (!ec && LaunchProcess(tracyPath.string())) {
                    SE_LOG_INFO("Launched Tracy Profiler");
                } else {
                    SE_LOG_ERROR("Failed to launch Tracy Profiler");
                }
            }
#else
            ImGui::MenuItem("Tracy Profiler (disabled)", nullptr, false, false);
#endif
            ImGui::EndMenu();
        }

        // ── Window control buttons (right-aligned, fullscreen only) ──
        if (m_window.IsFullscreen()) {
            const HWND hwnd = m_window.GetHandle();

            const float buttonW = ImGui::GetFrameHeight() * 1.4f;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float totalW = buttonW * 3.0f + spacing * 3.0f;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - totalW);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.8f));

            if (ImGui::Button("_##min", ImVec2(buttonW, 0))) {
                ::ShowWindow(hwnd, SW_MINIMIZE);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Minimize");
            }

            ImGui::SameLine();
            if (ImGui::Button("[=]##restore", ImVec2(buttonW, 0))) {
                m_window.ToggleFullscreen();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Restore");
            }

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button("X##close", ImVec2(buttonW, 0))) {
                ::PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Close");
            }
            ImGui::PopStyleColor(5);
        }

        ImGui::EndMainMenuBar();
    }
}

// ── Shutdown ─────────────────────────────────────────────────────

void EditorApp::Shutdown() {
    SE_ZONE_SCOPED;
    SaveEditorConfig();

    m_renderContext.GetDirectQueue().Flush();

    m_assetManager.Shutdown();

    m_scene.Clear();
    m_sceneRenderer.Shutdown();
    m_viewport.Shutdown(m_renderContext);
    m_imguiLayer.Shutdown();
    m_renderContext.Shutdown();
    m_window.Shutdown();
    SE_LOG_INFO("Editor shutdown");
}

// ── Play mode transitions ────────────────────────────────────────

void EditorApp::EnterPlayMode() {
    // Save editor camera state
    m_savedCameraPosition = m_viewport.GetCamera().GetPosition();
    m_savedCameraYaw = m_viewport.GetYaw();
    m_savedCameraPitch = m_viewport.GetPitch();

    m_mode = EditorMode::Play;

    // Update camera aspect ratio to match window (not viewport panel)
    float aspect = m_window.GetHeight() > 0 ? static_cast<float>(m_window.GetWidth()) / m_window.GetHeight() : 1.0f;
    auto& cam = m_viewport.GetCamera();
    cam.SetPerspective(cam.GetFovY(), aspect, cam.GetNearZ(), cam.GetFarZ());

    // Capture cursor for FPS controls
    HWND hwnd = m_window.GetHandle();
    SetCapture(hwnd);
    ShowCursor(FALSE);

    RECT rect;
    GetClientRect(hwnd, &rect);
    POINT center = {(rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2};
    POINT clientCenter = center;
    ClientToScreen(hwnd, &center);
    SetCursorPos(center.x, center.y);
    m_input.SetMousePosition(clientCenter.x, clientCenter.y);

    SE_LOG_INFO("Entered Play mode (F5)");
    SE_MESSAGE_L("Entered Play mode");
}

void EditorApp::ExitPlayMode() {
    m_mode = EditorMode::Edit;

    // Restore editor camera
    auto& cam = m_viewport.GetCamera();
    m_viewport.InitCamera(m_savedCameraPosition, m_savedCameraYaw, m_savedCameraPitch, cam.GetFovY(), cam.GetNearZ(),
                          cam.GetFarZ());

    // Release cursor
    ReleaseCapture();
    ShowCursor(TRUE);

    SE_LOG_INFO("Exited Play mode (ESC)");
    SE_MESSAGE_L("Exited Play mode");
}

// ── Main loop ────────────────────────────────────────────────────

int EditorApp::Run() {
    while (m_window.ProcessMessages()) {
        m_timer.Tick();
        SE_PLOT("Frame Time (ms)", m_timer.DeltaTime() * 1000.0f);
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

        // F11 to toggle fullscreen (works in both Edit and Play modes)
        if (m_input.IsKeyPressed(Key::kF11)) {
            m_window.ToggleFullscreen();
        }

        if (m_mode == EditorMode::Edit) {
            // F5 to enter play mode
            if (m_input.IsKeyPressed(Key::kF5)) {
                EnterPlayMode();
            }

            // Keyboard shortcuts (Ctrl+key)
            if (m_input.IsKeyDown(Key::kControl)) {
                if (m_input.IsKeyDown(Key::kShift) && m_input.IsKeyPressed('S')) {
                    SaveSceneAs();
                } else if (m_input.IsKeyDown(Key::kShift) && m_input.IsKeyPressed('Z')) {
                    m_commandHistory.Redo();
                } else if (m_input.IsKeyPressed('Z')) {
                    m_commandHistory.Undo();
                } else if (m_input.IsKeyPressed('Y')) {
                    m_commandHistory.Redo();
                } else if (m_input.IsKeyPressed('S')) {
                    SaveScene();
                } else if (m_input.IsKeyPressed('O')) {
                    m_pendingAction = PendingAction::OpenScene;
                } else if (m_input.IsKeyPressed('N')) {
                    m_pendingAction = PendingAction::NewScene;
                }
            }

            // Process deferred menu actions
            if (m_pendingAction == PendingAction::NewScene) {
                m_pendingAction = PendingAction::None;
                NewScene();
            } else if (m_pendingAction == PendingAction::OpenScene) {
                m_pendingAction = PendingAction::None;
                OpenScene();
            }

            // Update
            m_viewport.UpdateCamera(m_input, dt);
            m_editorController.Update({&m_input, &m_scene, &m_sceneRenderer, &m_viewport, &m_renderContext});

            // Begin render frame
            m_renderContext.BeginFrame();

            // Phase 1: Render scene to off-screen viewport render target
            m_viewport.BeginRender(m_renderContext.GetCommandList());
            m_sceneRenderer.Render(m_renderContext, m_viewport.GetCamera(), m_scene,
                                   m_editorController.GetSelectedObjectId());
            m_viewport.EndRender(m_renderContext.GetCommandList());

            // Render object IDs for GPU picking
            m_sceneRenderer.RenderObjectIds(m_renderContext, m_viewport.GetCamera(), m_scene);

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
            m_renderContext.EndFrame();

        } else {
            // ── Play mode ────────────────────────────────────────

            // ESC to exit play mode
            if (m_input.IsKeyPressed(Key::kEscape)) {
                ExitPlayMode();
                continue;
            }

            // Update camera with direct FPS controls (no right-click needed)
            m_viewport.UpdateCamera(m_input, dt, true);

            // Re-center cursor each frame to prevent edge-hitting
            HWND hwnd = m_window.GetHandle();
            RECT rect;
            GetClientRect(hwnd, &rect);
            POINT center = {(rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2};
            POINT clientCenter = center;
            ClientToScreen(hwnd, &center);
            SetCursorPos(center.x, center.y);
            m_input.SetMousePosition(clientCenter.x, clientCenter.y);

            // Render scene directly to back buffer
            m_renderContext.BeginFrame();

            float clearColor[] = {0.05f, 0.05f, 0.08f, 1.0f};
            m_renderContext.BeginBackBufferScenePass(clearColor);
            m_sceneRenderer.Render(m_renderContext, m_viewport.GetCamera(), m_scene, -1);
            m_renderContext.EndBackBufferPass();

            m_renderContext.EndFrame();
        }

        SE_FRAME_MARK;
    }

    return 0;
}

// ── Resize ────────────────────────────────────────────────────────

void EditorApp::OnResize(uint32_t width, uint32_t height) {
    SE_MESSAGE_L("Window resized");
    m_renderContext.Resize(width, height);

    // In play mode, update camera aspect to match window
    if (m_mode == EditorMode::Play && height > 0) {
        float aspect = static_cast<float>(width) / height;
        auto& cam = m_viewport.GetCamera();
        cam.SetPerspective(cam.GetFovY(), aspect, cam.GetNearZ(), cam.GetFarZ());
    }

    SE_LOG_INFO("Editor resized: {}x{}", width, height);
}

} // namespace showcase
