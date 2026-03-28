#pragma once

#include <showcase/graphics/Scene.h>
#include <showcase/graphics/SceneRenderer.h>

#include <imgui.h>
#include <ImGuizmo.h>

#include <functional>
#include <optional>

namespace showcase {

class CommandHistory;
class Input;
struct Model;
class RenderContext;
class ViewportPanel;

struct EditorUpdateDesc {
    const Input* input;
    Scene* scene;
    SceneRenderer* renderer;
    ViewportPanel* viewport;
    RenderContext* renderContext;
};

class EditorController {
public:
    void Update(const EditorUpdateDesc& desc);
    void RenderUI(Scene& scene, ViewportPanel& viewport);
    void RenderToolbar(ViewportPanel& viewport);

    int GetSelectedObjectId() const { return m_selectedObjectId; }
    void ClearSelection() { m_selectedObjectId = -1; }
    void SetSelection(uint32_t id) { m_selectedObjectId = static_cast<int>(id); }

    bool NeedsShadowPreview() const { return m_needsShadowPreview; }
    bool NeedsCubemapPreview() const { return m_needsCubemapPreview; }
    int GetCubemapPreviewShadowIndex() const { return m_cubemapPreviewShadowIndex; }

    PrimitiveHighlight GetPrimitiveHighlight() const {
        return {m_selectedObjectId, m_hoveredMeshIdx, m_hoveredPrimIdx};
    }

    using DirtyCallback = std::function<void()>;
    void SetDirtyCallback(DirtyCallback cb) { m_dirtyCallback = std::move(cb); }

    using AddObjectCallback = std::function<SceneObject*(const std::string& modelSource)>;
    void SetAddObjectCallback(AddObjectCallback cb) { m_addObjectCallback = std::move(cb); }

    using AssetListCallback = std::function<std::vector<std::string>()>;
    void SetAssetListCallback(AssetListCallback cb) { m_assetListCallback = std::move(cb); }

    using ResolveModelCallback = std::function<Model*(const std::string&)>;
    void SetResolveModelCallback(ResolveModelCallback cb) { m_resolveModelCallback = std::move(cb); }

    void SetCommandHistory(CommandHistory* history) { m_commandHistory = history; }
    int& GetSelectedObjectIdRef() { return m_selectedObjectId; }

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
    int m_hoveredMeshIdx = -1;
    int m_hoveredPrimIdx = -1;

    // Viewport picking state (1-frame delayed from OnImGui)
    ImVec2 m_viewportMin = {};
    ImVec2 m_viewportMax = {};
    bool m_viewportHovered = false;
    bool m_pickPending = false;

    // Gizmo
    ImGuizmo::OPERATION m_gizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_gizmoMode = ImGuizmo::LOCAL;
    bool m_useSnap = false;
    float m_snapTranslate = 1.0f;
    float m_snapRotate = 15.0f;
    float m_snapScale = 0.1f;

    DirtyCallback m_dirtyCallback;
    AddObjectCallback m_addObjectCallback;
    AssetListCallback m_assetListCallback;
    ResolveModelCallback m_resolveModelCallback;

    CommandHistory* m_commandHistory = nullptr;
    SceneRenderer* m_renderer = nullptr;
    bool m_needsShadowPreview = false;
    bool m_needsCubemapPreview = false;
    int m_cubemapPreviewShadowIndex = -1;

    // Gizmo drag coalescing
    bool m_gizmoDragging = false;
    Vector3 m_gizmoStartPos;
    Vector3 m_gizmoStartRot;
    Vector3 m_gizmoStartScale;

    // Inspector drag start values
    Vector3 m_dragStartPos;
    Vector3 m_dragStartRot;
    Vector3 m_dragStartScale;
    std::string m_dragStartName;
    std::optional<Vector4> m_dragStartColor;
    LightComponent m_dragStartLightProps;

    struct DragLightPropertyDesc {
        const char* label;
        float* value;
        float speed;
        float min;
        float max;
        LightComponent* light;
        Scene* scene;
        uint32_t objectId;
    };
    void DragLightProperty(const DragLightPropertyDesc& desc);
    void DuplicateSelectedObject(Scene& scene);
};

} // namespace showcase
