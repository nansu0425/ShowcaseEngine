#pragma once

#include <showcase/core/Window.h>
#include <showcase/core/Timer.h>
#include <showcase/core/Input.h>
#include <showcase/graphics/RenderContext.h>
#include <showcase/ui/ImGuiLayer.h>
#include <showcase/ui/OverlaySystem.h>
#include <showcase/ui/LogConsole.h>
#include <showcase/ui/Viewport.h>
#include <showcase/demo/ShowcaseManager.h>

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
    ShowcaseManager& GetShowcaseManager() { return m_showcaseManager; }

private:
    void OnResize(uint32_t width, uint32_t height);

    Window m_window;
    Timer m_timer;
    Input m_input;
    RenderContext m_renderContext;
    ImGuiLayer m_imguiLayer;
    OverlaySystem m_overlay;
    LogConsole m_logConsole;
    Viewport m_viewport;
    ShowcaseManager m_showcaseManager;

    bool m_resizePending = false;
    uint32_t m_pendingWidth = 0;
    uint32_t m_pendingHeight = 0;
};

} // namespace showcase
