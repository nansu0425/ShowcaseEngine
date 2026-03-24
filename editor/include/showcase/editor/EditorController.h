#pragma once

#include <showcase/graphics/Scene.h>
#include <showcase/graphics/SceneRenderer.h>

#include <imgui.h>
#include <ImGuizmo.h>

#include <functional>

namespace showcase {

class Input;
class ViewportPanel;

class EditorController {
public:
    void Update(const Input& input, Scene& scene, SceneRenderer& renderer, ViewportPanel& viewport);
    void RenderUI(Scene& scene, ViewportPanel& viewport);
    void RenderToolbar(ViewportPanel& viewport);

    int GetSelectedObjectId() const { return m_selectedObjectId; }
    void ClearSelection() { m_selectedObjectId = -1; }
    void SetSelection(uint32_t id) { m_selectedObjectId = static_cast<int>(id); }

    using DirtyCallback = std::function<void()>;
    void SetDirtyCallback(DirtyCallback cb) { m_dirtyCallback = std::move(cb); }

    using AddObjectCallback = std::function<SceneObject*(const std::string& modelSource)>;
    void SetAddObjectCallback(AddObjectCallback cb) { m_addObjectCallback = std::move(cb); }

    // State persistence
    ImGuizmo::OPERATION GetGizmoOperation() const { return m_gizmoOperation; }
    ImGuizmo::MODE GetGizmoMode() const { return m_gizmoMode; }
    bool GetUseSnap() const { return m_useSnap; }
    float GetSnapTranslate() const { return m_snapTranslate; }
    float GetSnapRotate() const { return m_snapRotate; }
    float GetSnapScale() const { return m_snapScale; }

    void SetGizmoOperation(ImGuizmo::OPERATION op) { m_gizmoOperation = op; }
    void SetGizmoMode(ImGuizmo::MODE mode) { m_gizmoMode = mode; }
    void SetUseSnap(bool snap) { m_useSnap = snap; }
    void SetSnapTranslate(float v) { m_snapTranslate = v; }
    void SetSnapRotate(float v) { m_snapRotate = v; }
    void SetSnapScale(float v) { m_snapScale = v; }

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

    DirtyCallback m_dirtyCallback;
    AddObjectCallback m_addObjectCallback;
};

} // namespace showcase
