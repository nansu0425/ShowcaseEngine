#include <showcase/editor/EditorController.h>

#include <showcase/core/Input.h>
#include <showcase/editor/ViewportPanel.h>

#include <imgui.h>
#include <imgui_internal.h>

namespace showcase {

// ── Update ────────────────────────────────────────────────────────

void EditorController::Update(const Input &input, Scene &scene, SceneRenderer &renderer,
                              ViewportPanel &viewport) {
    Camera &camera = viewport.GetCamera();

    // Left-click picking (not during right-click camera rotation, not when using gizmo)
    if (input.IsMouseButtonPressed(0) && !input.IsMouseButtonDown(1) && m_viewportHovered &&
        !ImGuizmo::IsOver()) {
        m_selectedObjectId =
            renderer.PickObject(input.GetMouseX(), input.GetMouseY(), camera, scene,
                                m_viewportMin.x, m_viewportMin.y, m_viewportMax.x, m_viewportMax.y);
    }

    // Gizmo shortcuts (only when right-click not held, to avoid camera movement conflict)
    if (!input.IsMouseButtonDown(1)) {
        if (input.IsKeyPressed('W')) {
            m_gizmoOperation = ImGuizmo::TRANSLATE;
        }
        if (input.IsKeyPressed('E')) {
            m_gizmoOperation = ImGuizmo::ROTATE;
        }
        if (input.IsKeyPressed('R')) {
            m_gizmoOperation = ImGuizmo::SCALE;
        }
        if (input.IsKeyPressed('X')) {
            m_gizmoMode = (m_gizmoMode == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
        }
    }
}

// ── UI ────────────────────────────────────────────────────────────

void EditorController::RenderUI(Scene &scene, ViewportPanel &viewport) {
    Camera &camera = viewport.GetCamera();

    ImGuizmo::BeginFrame();

    // -- Viewport hover detection (uses previous frame's image rect) --
    m_viewportMin = viewport.GetImageMin();
    m_viewportMax = viewport.GetImageMax();
    m_viewportHovered = ImGui::IsMouseHoveringRect(m_viewportMin, m_viewportMax, false);

    // -- Gizmo rendering --
    auto *vpWindow = ImGui::FindWindowByName("Viewport");
    if (vpWindow && !vpWindow->Hidden && m_selectedObjectId > 0) {
        SceneObject *selected = scene.FindById(static_cast<uint32_t>(m_selectedObjectId));
        if (selected) {
            ImGuizmo::SetDrawlist(vpWindow->DrawList);
            ImGuizmo::SetRect(m_viewportMin.x, m_viewportMin.y, m_viewportMax.x - m_viewportMin.x,
                              m_viewportMax.y - m_viewportMin.y);

            Matrix view = camera.GetViewMatrix();
            Matrix proj = camera.GetProjectionMatrix();
            Matrix world = selected->worldTransform;

            // ImGuizmo assumes RH coordinates. Our LH view matrix has Z basis
            // pointing into the scene; RH expects it pointing away. Negate view
            // column 2 and proj row 2 — the two flips cancel in view*proj (so
            // screen-space rendering is unchanged) while the camera-direction
            // extracted from inverse(view) matches RH expectations.
            view._13 = -view._13;
            view._23 = -view._23;
            view._33 = -view._33;
            view._43 = -view._43;
            proj._31 = -proj._31;
            proj._32 = -proj._32;
            proj._33 = -proj._33;
            proj._34 = -proj._34;

            float snap[3] = {0.0f, 0.0f, 0.0f};
            if (m_useSnap) {
                float v = (m_gizmoOperation == ImGuizmo::ROTATE)  ? m_snapRotate
                          : (m_gizmoOperation == ImGuizmo::SCALE) ? m_snapScale
                                                                  : m_snapTranslate;
                snap[0] = snap[1] = snap[2] = v;
            }

            ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], m_gizmoOperation, m_gizmoMode,
                                 &world.m[0][0], nullptr, m_useSnap ? snap : nullptr);

            if (ImGuizmo::IsUsing()) {
                Vector3 newPos, newScale;
                Quaternion newRot;
                if (world.Decompose(newScale, newRot, newPos)) {
                    selected->position = newPos;
                    selected->scale = ClampScale(newScale);

                    Vector3 euler = newRot.ToEuler();
                    selected->rotation =
                        Vector3(ToDegrees(euler.x), ToDegrees(euler.y), ToDegrees(euler.z));

                    selected->RecomputeWorldTransform();
                    selected->UpdateAABB();

                    if (m_dirtyCallback) m_dirtyCallback();
                }
            }
        }
    }

    // -- Scene Hierarchy panel --
    if (ImGui::Begin("Scene Hierarchy")) {
        for (auto &obj : scene.GetObjects()) {
            bool isSelected = (static_cast<int>(obj.id) == m_selectedObjectId);
            if (ImGui::Selectable(obj.name.c_str(), isSelected)) {
                m_selectedObjectId = static_cast<int>(obj.id);
            }
        }
    }
    ImGui::End();

    // -- Inspector panel --
    if (ImGui::Begin("Inspector")) {
        SceneObject *selected = (m_selectedObjectId > 0)
                                    ? scene.FindById(static_cast<uint32_t>(m_selectedObjectId))
                                    : nullptr;
        if (selected) {
            ImGui::Text("Name: %s", selected->name.c_str());
            ImGui::Text("ID: %u", selected->id);
            ImGui::Separator();

            bool changed = false;
            if (ImGui::DragFloat3("Position", &selected->position.x, 0.1f)) {
                changed = true;
            }
            if (ImGui::DragFloat3("Rotation", &selected->rotation.x, 1.0f)) {
                changed = true;
            }
            if (ImGui::DragFloat3("Scale", &selected->scale.x, 0.1f, 0.01f, 100.0f)) {
                selected->scale = ClampScale(selected->scale);
                changed = true;
            }

            if (changed) {
                selected->RecomputeWorldTransform();
                selected->UpdateAABB();
                if (m_dirtyCallback) m_dirtyCallback();
            }
        } else {
            ImGui::TextDisabled("No object selected");
        }
    }
    ImGui::End();
}

// ── Toolbar ───────────────────────────────────────────────────────

void EditorController::RenderToolbar(ViewportPanel &viewport) {
    Camera &camera = viewport.GetCamera();

    if (ImGui::RadioButton("Translate", m_gizmoOperation == ImGuizmo::TRANSLATE)) {
        m_gizmoOperation = ImGuizmo::TRANSLATE;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Move objects along axes (W)");
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", m_gizmoOperation == ImGuizmo::ROTATE)) {
        m_gizmoOperation = ImGuizmo::ROTATE;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Rotate objects around axes (E)");
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", m_gizmoOperation == ImGuizmo::SCALE)) {
        m_gizmoOperation = ImGuizmo::SCALE;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Scale objects along axes (R)");
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    if (ImGui::RadioButton("Local", m_gizmoMode == ImGuizmo::LOCAL)) {
        m_gizmoMode = ImGuizmo::LOCAL;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Transform relative to object's own axes (X)");
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("World", m_gizmoMode == ImGuizmo::WORLD)) {
        m_gizmoMode = ImGuizmo::WORLD;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Transform relative to world axes (X)");
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::Checkbox("Snap", &m_useSnap);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Snap transform values to fixed increments");
    }
    if (m_useSnap) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (m_gizmoOperation == ImGuizmo::TRANSLATE) {
            ImGui::DragFloat("##SnapVal", &m_snapTranslate, 0.1f, 0.1f, 10.0f);
        } else if (m_gizmoOperation == ImGuizmo::ROTATE) {
            ImGui::DragFloat("##SnapVal", &m_snapRotate, 1.0f, 1.0f, 90.0f);
        } else {
            ImGui::DragFloat("##SnapVal", &m_snapScale, 0.01f, 0.01f, 1.0f);
        }
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    if (ImGui::Button("Camera")) {
        ImGui::OpenPopup("CameraSettings");
    }

    if (ImGui::BeginPopup("CameraSettings")) {
        ImGui::Text("Position: (%.1f, %.1f, %.1f)", camera.GetPosition().x, camera.GetPosition().y,
                    camera.GetPosition().z);
        ImGui::SliderFloat("Move Speed", &viewport.cameraMoveSpeed, 1.0f, 50.0f);
        ImGui::SliderFloat("Look Sensitivity", &viewport.cameraLookSpeed, 0.001f, 0.01f);
        ImGui::EndPopup();
    }
}

} // namespace showcase
