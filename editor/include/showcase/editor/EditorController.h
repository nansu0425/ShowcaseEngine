#pragma once

#include <showcase/graphics/Scene.h>
#include <showcase/graphics/SceneRenderer.h>

#include <imgui.h>
#include <ImGuizmo.h>

namespace showcase {

class Input;
class ViewportPanel;

class EditorController {
public:
    void Update(const Input& input, Scene& scene, SceneRenderer& renderer, ViewportPanel& viewport);
    void RenderUI(Scene& scene, ViewportPanel& viewport);
    void RenderToolbar(ViewportPanel& viewport);

    int GetSelectedObjectId() const { return m_selectedObjectId; }

private:
    int m_selectedObjectId = -1;

    // Viewport picking state (1-frame delayed from OnImGui)
    ImVec2 m_viewportMin = {};
    ImVec2 m_viewportMax = {};
    bool m_viewportHovered = false;

    // Gizmo
    ImGuizmo::OPERATION m_gizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_gizmoMode = ImGuizmo::LOCAL;
    bool m_useSnap = false;
    float m_snapTranslate = 1.0f;
    float m_snapRotate = 15.0f;
    float m_snapScale = 0.1f;
};

} // namespace showcase
