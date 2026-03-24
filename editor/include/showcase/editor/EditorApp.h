#pragma once

#include <showcase/core/Input.h>
#include <showcase/core/Timer.h>
#include <showcase/core/Window.h>
#include <showcase/editor/Console.h>
#include <showcase/editor/EditorController.h>
#include <showcase/editor/ImGuiLayer.h>
#include <showcase/editor/ViewportPanel.h>
#include <showcase/graphics/AssetManager.h>
#include <showcase/graphics/Model.h>
#include <showcase/graphics/RenderContext.h>
#include <showcase/graphics/Scene.h>
#include <showcase/graphics/SceneRenderer.h>

#include <memory>

namespace showcase {

struct AssetEntry {
    std::string relativePath; // e.g. "assets/models/BoxTextured.glb"
    std::string filename;     // e.g. "BoxTextured.glb"
    std::string extension;    // e.g. ".glb"
};

enum class EditorMode {
    Edit,
    Play
};

struct EditorAppDesc {
    WindowDesc window;
};

class EditorApp {
public:
    [[nodiscard]] bool Init(const EditorAppDesc& desc);
    void Shutdown();
    int Run();

    Window& GetWindow() { return m_window; }
    RenderContext& GetRenderContext() { return m_renderContext; }

private:
    void OnResize(uint32_t width, uint32_t height);
    void BuildDefaultScene();
    void SaveEditorConfig();
    void LoadEditorConfig();

    // Scene document management
    void NewScene();
    void OpenScene();
    bool SaveScene();
    bool SaveSceneAs();
    void RenderMainMenuBar();
    void UpdateWindowTitle();
    void ResolveSceneModels();
    void ScanAssets();
    void ImportAsset();

    Window m_window;
    Timer m_timer;
    Input m_input;
    RenderContext m_renderContext;
    ImGuiLayer m_imguiLayer;
    Console m_console;
    ViewportPanel m_viewport;

    // Scene rendering
    SceneRenderer m_sceneRenderer;
    Scene m_scene;
    EditorController m_editorController;

    AssetManager m_assetManager;

    bool m_resizePending = false;
    uint32_t m_pendingWidth = 0;
    uint32_t m_pendingHeight = 0;

    // Scene document state
    std::string m_currentScenePath;
    bool m_sceneDirty = false;
    std::vector<AssetEntry> m_availableAssets;

    // Deferred scene actions (menu items run mid-frame; GPU resource changes must happen between frames)
    enum class PendingAction {
        None,
        NewScene,
        OpenScene
    };
    PendingAction m_pendingAction = PendingAction::None;

    // Play mode
    EditorMode m_mode = EditorMode::Edit;
    Vector3 m_savedCameraPosition;
    float m_savedCameraYaw = 0.0f;
    float m_savedCameraPitch = 0.0f;

    void EnterPlayMode();
    void ExitPlayMode();
};

} // namespace showcase
