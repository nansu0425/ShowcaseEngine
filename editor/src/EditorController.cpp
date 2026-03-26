#include <showcase/editor/EditorController.h>

#include <showcase/core/Input.h>
#include <showcase/core/Profiler.h>
#include <showcase/editor/CommandHistory.h>
#include <showcase/editor/ViewportPanel.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>

namespace showcase {

// ── Update ────────────────────────────────────────────────────────

void EditorController::Update(const EditorUpdateDesc& desc) {
    SE_ZONE_SCOPED_C(profile::kColorUpdate);
    const Input& input = *desc.input;
    Scene& scene = *desc.scene;
    SceneRenderer& renderer = *desc.renderer;
    ViewportPanel& viewport = *desc.viewport;
    RenderContext& renderContext = *desc.renderContext;

    // Check for completed GPU pick result from previous frame
    if (m_pickPending && renderer.IsPickComplete(renderContext)) {
        m_selectedObjectId = renderer.GetPickResult(renderContext);
        m_pickPending = false;
    }

    // Left-click picking (not during right-click camera rotation, not when using gizmo)
    if (input.IsMouseButtonPressed(0) && !input.IsMouseButtonDown(1) && m_viewportHovered && !ImGuizmo::IsOver()) {
        // Convert screen coordinates to ID RT pixel coordinates
        float vpWidth = m_viewportMax.x - m_viewportMin.x;
        float vpHeight = m_viewportMax.y - m_viewportMin.y;
        if (vpWidth > 0 && vpHeight > 0) {
            float localX = static_cast<float>(input.GetMouseX()) - m_viewportMin.x;
            float localY = static_cast<float>(input.GetMouseY()) - m_viewportMin.y;
            float normX = localX / vpWidth;
            float normY = localY / vpHeight;

            int pixelX = static_cast<int>(normX * viewport.GetWidth());
            int pixelY = static_cast<int>(normY * viewport.GetHeight());

            pixelX = std::clamp(pixelX, 0, static_cast<int>(viewport.GetWidth()) - 1);
            pixelY = std::clamp(pixelY, 0, static_cast<int>(viewport.GetHeight()) - 1);

            renderer.RequestPick(pixelX, pixelY);
            m_pickPending = true;
        }
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
        if (input.IsKeyPressed(Key::kDelete) && m_selectedObjectId > 0) {
            if (m_commandHistory) {
                m_commandHistory->ExecuteCommand(std::make_unique<RemoveObjectCommand>(
                    scene, m_selectedObjectId, static_cast<uint32_t>(m_selectedObjectId)));
            }
        }
    }
}

// ── Primitive Details (Model Info panel helper) ──────────────────

static void RenderPrimitiveDetails(const MeshPrimitive& prim) {
    ImGui::Text("Triangles: %u", prim.indexCount / 3);

    if (prim.material) {
        char matLabel[256];
        snprintf(matLabel, sizeof(matLabel), "Material [%d] %s", prim.materialIndex,
                 prim.material->name.empty() ? "(unnamed)" : prim.material->name.c_str());
        if (ImGui::TreeNode(matLabel)) {
            const auto& c = prim.material->baseColorFactor;
            ImGui::ColorButton("##baseColor", ImVec4(c.x, c.y, c.z, c.w), ImGuiColorEditFlags_NoTooltip);
            ImGui::SameLine();
            ImGui::Text("Base Color: (%.2f, %.2f, %.2f, %.2f)", c.x, c.y, c.z, c.w);

            if (prim.material->baseColorTexture) {
                uint32_t w = prim.material->baseColorTexture->GetWidth();
                uint32_t h = prim.material->baseColorTexture->GetHeight();
                const auto& uri = prim.material->baseColorTexture->GetSourceURI();
                if (!uri.empty()) {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Texture: %ux%u [%s]", w, h, uri.c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Texture: %ux%u", w, h);
                }

                // Texture preview
                D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = prim.material->baseColorTexture->GetSRVHandle().gpu;
                float previewSize = 128.0f;
                ImGui::Image((ImTextureID)gpuHandle.ptr, ImVec2(previewSize, previewSize));
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Texture: No Texture");
            }

            if (prim.material->alphaMode == AlphaMode::Mask) {
                ImGui::Text("Alpha: Mask (cutoff: %.2f)", prim.material->alphaCutoff);
            } else if (prim.material->alphaMode == AlphaMode::Blend) {
                ImGui::Text("Alpha: Blend");
            } else {
                ImGui::Text("Alpha: Opaque");
            }

            if (prim.material->doubleSided)
                ImGui::Text("Double-sided: Yes");

            ImGui::TreePop();
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "No Material");
    }
}

// ── UI ────────────────────────────────────────────────────────────

void EditorController::RenderUI(Scene& scene, ViewportPanel& viewport) {
    SE_ZONE_SCOPED_C(profile::kColorImGui);
    Camera& camera = viewport.GetCamera();

    ImGuizmo::BeginFrame();

    // -- Viewport hover detection (uses previous frame's image rect) --
    m_viewportMin = viewport.GetImageMin();
    m_viewportMax = viewport.GetImageMax();
    m_viewportHovered = ImGui::IsMouseHoveringRect(m_viewportMin, m_viewportMax, false);

    // Clear any ImGui widget focus when right-clicking viewport (e.g., console input)
    if (m_viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::FocusWindow(nullptr);
    }

    // -- Gizmo rendering --
    auto* vpWindow = ImGui::FindWindowByName("Viewport");
    if (vpWindow && !vpWindow->Hidden && m_selectedObjectId > 0) {
        SceneObject* selected = scene.FindById(static_cast<uint32_t>(m_selectedObjectId));
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

            ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], m_gizmoOperation, m_gizmoMode, &world.m[0][0], nullptr,
                                 m_useSnap ? snap : nullptr);

            if (ImGuizmo::IsUsing()) {
                if (!m_gizmoDragging) {
                    m_gizmoDragging = true;
                    m_gizmoStartPos = selected->position;
                    m_gizmoStartRot = selected->rotation;
                    m_gizmoStartScale = selected->scale;
                }

                Vector3 newPos, newScale;
                Quaternion newRot;
                if (world.Decompose(newScale, newRot, newPos)) {
                    selected->position = newPos;
                    selected->scale = ClampScale(newScale);

                    Vector3 euler = newRot.ToEuler();
                    selected->rotation = Vector3(ToDegrees(euler.x), ToDegrees(euler.y), ToDegrees(euler.z));

                    selected->RecomputeWorldTransform();
                    selected->UpdateAABB();

                    if (m_dirtyCallback)
                        m_dirtyCallback();
                }
            } else if (m_gizmoDragging) {
                m_gizmoDragging = false;
                if (m_commandHistory) {
                    m_commandHistory->RecordCommand(std::make_unique<TransformCommand>(
                        TransformCommandDesc{&scene, selected->id, m_gizmoStartPos, m_gizmoStartRot, m_gizmoStartScale,
                                             selected->position, selected->rotation, selected->scale}));
                }
            }
        }
    }

    // -- Scene Hierarchy panel --
    if (ImGui::Begin("Scene Hierarchy")) {
        if (ImGui::Button("+")) {
            if (m_addObjectCallback) {
                SceneObject* newObj = m_addObjectCallback("");
                if (newObj && m_commandHistory) {
                    m_commandHistory->ExecuteCommand(
                        std::make_unique<AddObjectCommand>(scene, m_selectedObjectId, *newObj));
                }
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Add object to scene");
        }
        ImGui::SameLine();
        const bool hasSelection = m_selectedObjectId > 0;
        if (!hasSelection)
            ImGui::BeginDisabled();
        if (ImGui::Button("-")) {
            if (m_commandHistory) {
                m_commandHistory->ExecuteCommand(std::make_unique<RemoveObjectCommand>(
                    scene, m_selectedObjectId, static_cast<uint32_t>(m_selectedObjectId)));
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Remove selected object (Del)");
        }
        if (!hasSelection)
            ImGui::EndDisabled();

        ImGui::Separator();

        for (auto& obj : scene.GetObjects()) {
            ImGui::PushID(static_cast<int>(obj.id));
            bool isSelected = (static_cast<int>(obj.id) == m_selectedObjectId);
            if (ImGui::Selectable(obj.name.c_str(), isSelected)) {
                m_selectedObjectId = static_cast<int>(obj.id);
            }
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete")) {
                    if (m_commandHistory) {
                        m_commandHistory->ExecuteCommand(
                            std::make_unique<RemoveObjectCommand>(scene, m_selectedObjectId, obj.id));
                    }
                    ImGui::EndPopup();
                    ImGui::PopID();
                    break;
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }
    ImGui::End();

    // -- Inspector panel --
    if (ImGui::Begin("Inspector")) {
        SceneObject* selected =
            (m_selectedObjectId > 0) ? scene.FindById(static_cast<uint32_t>(m_selectedObjectId)) : nullptr;
        if (selected) {
            char nameBuf[256];
            strncpy_s(nameBuf, selected->name.c_str(), sizeof(nameBuf) - 1);
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                selected->name = nameBuf;
                if (m_dirtyCallback)
                    m_dirtyCallback();
            }
            if (ImGui::IsItemActivated()) {
                m_dragStartName = selected->name;
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                if (m_commandHistory && selected->name != m_dragStartName) {
                    m_commandHistory->RecordCommand(
                        std::make_unique<RenameCommand>(scene, selected->id, m_dragStartName, selected->name));
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("ID: %u", selected->id);
            ImGui::Separator();

            // ── Transform (always shown) ──
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                bool changed = false;

                if (ImGui::DragFloat3("Position", &selected->position.x, 0.1f)) {
                    changed = true;
                }
                if (ImGui::IsItemActivated()) {
                    m_dragStartPos = selected->position;
                    m_dragStartRot = selected->rotation;
                    m_dragStartScale = selected->scale;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    if (m_commandHistory) {
                        m_commandHistory->RecordCommand(std::make_unique<TransformCommand>(
                            TransformCommandDesc{&scene, selected->id, m_dragStartPos, m_dragStartRot, m_dragStartScale,
                                                 selected->position, selected->rotation, selected->scale}));
                    }
                }

                if (ImGui::DragFloat3("Rotation", &selected->rotation.x, 1.0f)) {
                    changed = true;
                }
                if (ImGui::IsItemActivated()) {
                    m_dragStartPos = selected->position;
                    m_dragStartRot = selected->rotation;
                    m_dragStartScale = selected->scale;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    if (m_commandHistory) {
                        m_commandHistory->RecordCommand(std::make_unique<TransformCommand>(
                            TransformCommandDesc{&scene, selected->id, m_dragStartPos, m_dragStartRot, m_dragStartScale,
                                                 selected->position, selected->rotation, selected->scale}));
                    }
                }

                if (ImGui::DragFloat3("Scale", &selected->scale.x, 0.1f, 0.01f, 100.0f)) {
                    selected->scale = ClampScale(selected->scale);
                    changed = true;
                }
                if (ImGui::IsItemActivated()) {
                    m_dragStartPos = selected->position;
                    m_dragStartRot = selected->rotation;
                    m_dragStartScale = selected->scale;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    if (m_commandHistory) {
                        m_commandHistory->RecordCommand(std::make_unique<TransformCommand>(
                            TransformCommandDesc{&scene, selected->id, m_dragStartPos, m_dragStartRot, m_dragStartScale,
                                                 selected->position, selected->rotation, selected->scale}));
                    }
                }

                if (changed) {
                    selected->RecomputeWorldTransform();
                    selected->UpdateAABB();
                    if (m_dirtyCallback)
                        m_dirtyCallback();
                }
            }

            // ── Mesh Component ──
            if (selected->modelComp.has_value()) {
                bool headerOpen = true;
                if (ImGui::CollapsingHeader("Model Component", &headerOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                    std::string currentSource = selected->modelComp->modelSource;
                    const char* preview = currentSource.empty() ? "(none)" : currentSource.c_str();

                    if (ImGui::BeginCombo("Model", preview)) {
                        // "(none)" option to clear model
                        if (ImGui::Selectable("(none)", currentSource.empty())) {
                            if (m_commandHistory) {
                                m_commandHistory->ExecuteCommand(
                                    std::make_unique<ChangeModelCommand>(ChangeModelCommandDesc{
                                        &scene, selected->id, currentSource, "", m_resolveModelCallback}));
                            }
                        }

                        // List available assets
                        if (m_assetListCallback) {
                            auto sources = m_assetListCallback();
                            for (const auto& src : sources) {
                                bool isCurrent = (src == currentSource);
                                if (ImGui::Selectable(src.c_str(), isCurrent)) {
                                    if (m_commandHistory) {
                                        m_commandHistory->ExecuteCommand(
                                            std::make_unique<ChangeModelCommand>(ChangeModelCommandDesc{
                                                &scene, selected->id, currentSource, src, m_resolveModelCallback}));
                                    }
                                }
                                if (isCurrent)
                                    ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    // ── Model Info ──
                    if (selected->modelComp->model) {
                        const auto* model = selected->modelComp->model;
                        ImGui::Spacing();
                        ImGui::SeparatorText("Model Info");

                        // Reset hover state each frame
                        m_hoveredMeshIdx = -1;
                        m_hoveredPrimIdx = -1;

                        uint32_t totalPrims = 0;
                        for (const auto& mesh : model->meshes) {
                            totalPrims += static_cast<uint32_t>(mesh.primitives.size());
                        }
                        ImGui::Text("Meshes: %zu | Primitives: %u", model->meshes.size(), totalPrims);

                        for (size_t mi = 0; mi < model->meshes.size(); ++mi) {
                            const auto& mesh = model->meshes[mi];
                            const char* meshName = mesh.name.empty() ? "(unnamed)" : mesh.name.c_str();
                            char meshLabel[256];
                            snprintf(meshLabel, sizeof(meshLabel), "[%zu] %s (%zu primitive%s)", mi, meshName,
                                     mesh.primitives.size(), mesh.primitives.size() == 1 ? "" : "s");

                            if (ImGui::TreeNode(meshLabel)) {
                                for (size_t pi = 0; pi < mesh.primitives.size(); ++pi) {
                                    ImGui::PushID(static_cast<int>(pi));
                                    char primLabel[128];
                                    snprintf(primLabel, sizeof(primLabel), "Primitive %zu", pi);
                                    if (ImGui::TreeNode(primLabel)) {
                                        RenderPrimitiveDetails(mesh.primitives[pi]);
                                        ImGui::TreePop();
                                    }
                                    if (ImGui::IsItemHovered()) {
                                        m_hoveredMeshIdx = static_cast<int>(mi);
                                        m_hoveredPrimIdx = static_cast<int>(pi);
                                    }
                                    ImGui::PopID();
                                }
                                ImGui::TreePop();
                            }
                        }
                    }

                    // ── Material Override ──
                    ImGui::Spacing();
                    ImGui::SeparatorText("Material Override");

                    bool hasOverride = selected->modelComp->baseColor.has_value();
                    if (ImGui::Checkbox("Base Color Override", &hasOverride)) {
                        std::optional<Vector4> oldColor = selected->modelComp->baseColor;
                        std::optional<Vector4> newColor =
                            hasOverride ? std::optional(Vector4(0.7f, 0.7f, 0.7f, 1.0f)) : std::nullopt;
                        if (m_commandHistory) {
                            m_commandHistory->ExecuteCommand(std::make_unique<ChangeBaseColorCommand>(
                                ChangeBaseColorCommandDesc{&scene, selected->id, oldColor, newColor}));
                        }
                    }
                    if (selected->modelComp->baseColor.has_value()) {
                        float color[3] = {selected->modelComp->baseColor->x, selected->modelComp->baseColor->y,
                                          selected->modelComp->baseColor->z};
                        if (ImGui::ColorEdit3("Color", color)) {
                            selected->modelComp->baseColor = Vector4(color[0], color[1], color[2], 1.0f);
                            if (m_dirtyCallback)
                                m_dirtyCallback();
                        }
                        if (ImGui::IsItemActivated()) {
                            m_dragStartColor = selected->modelComp->baseColor;
                        }
                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            if (m_commandHistory) {
                                m_commandHistory->RecordCommand(
                                    std::make_unique<ChangeBaseColorCommand>(ChangeBaseColorCommandDesc{
                                        &scene, selected->id, m_dragStartColor, selected->modelComp->baseColor}));
                            }
                        }
                    }
                }

                // Remove component if header close button was clicked
                if (!headerOpen) {
                    if (m_commandHistory) {
                        m_commandHistory->ExecuteCommand(std::make_unique<AddComponentCommand>(
                            AddComponentCommandDesc{&scene, selected->id, selected->modelComp, std::nullopt}));
                    }
                }
            }

            // ── Add Component button ──
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("+ Add Component")) {
                ImGui::OpenPopup("AddComponentPopup");
            }
            if (ImGui::BeginPopup("AddComponentPopup")) {
                if (!selected->modelComp.has_value()) {
                    if (ImGui::MenuItem("Model")) {
                        if (m_commandHistory) {
                            m_commandHistory->ExecuteCommand(std::make_unique<AddComponentCommand>(
                                AddComponentCommandDesc{&scene, selected->id, std::nullopt, ModelComponent{}}));
                        }
                    }
                } else {
                    ImGui::TextDisabled("All components added");
                }
                ImGui::EndPopup();
            }

        } else {
            ImGui::TextDisabled("No object selected");
        }
    }
    ImGui::End();
}

// ── Toolbar ───────────────────────────────────────────────────────

void EditorController::RenderToolbar(ViewportPanel& viewport) {
    Camera& camera = viewport.GetCamera();

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
