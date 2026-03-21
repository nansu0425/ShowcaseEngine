#pragma once

#include <showcase/core/Window.h>
#include <showcase/core/Timer.h>
#include <showcase/core/Input.h>
#include <showcase/core/FPSCameraController.h>
#include <showcase/graphics/RenderContext.h>
#include <showcase/graphics/Camera.h>
#include <showcase/graphics/Model.h>
#include <showcase/graphics/Scene.h>
#include <showcase/graphics/SceneRenderer.h>
#include <showcase/ui/ImGuiLayer.h>
#include <showcase/ui/Console.h>
#include <showcase/ui/Viewport.h>
#include <showcase/ui/EditorController.h>

namespace showcase {

struct ApplicationDesc {
    WindowDesc window;
};

class Application {
public:
    bool Init(const ApplicationDesc& desc);
    void Shutdown();
    int Run();

    Window& GetWindow() { return m_window; }
    RenderContext& GetRenderContext() { return m_renderContext; }

private:
    void OnResize(uint32_t width, uint32_t height);
    void BuildDefaultScene();

    Window m_window;
    Timer m_timer;
    Input m_input;
    RenderContext m_renderContext;
    ImGuiLayer m_imguiLayer;
    Console m_console;
    Viewport m_viewport;

    // Scene rendering
    SceneRenderer m_sceneRenderer;
    Scene m_scene;
    Camera m_camera;
    FPSCameraController m_cameraController;
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
