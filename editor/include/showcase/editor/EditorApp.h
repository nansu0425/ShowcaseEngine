#pragma once

#include <showcase/core/Window.h>
#include <showcase/core/Timer.h>
#include <showcase/core/Input.h>
#include <showcase/graphics/RenderContext.h>
#include <showcase/graphics/Model.h>
#include <showcase/graphics/Scene.h>
#include <showcase/graphics/SceneRenderer.h>
#include <showcase/editor/ImGuiLayer.h>
#include <showcase/editor/Console.h>
#include <showcase/editor/ViewportPanel.h>
#include <showcase/editor/EditorController.h>

namespace showcase {

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

    // Procedural models
    Model m_gridModel;
    Model m_cubeModel;

    // glTF model
    Model m_testModel;
    bool m_modelLoaded = false;

    bool m_resizePending = false;
    uint32_t m_pendingWidth = 0;
    uint32_t m_pendingHeight = 0;
};

} // namespace showcase
