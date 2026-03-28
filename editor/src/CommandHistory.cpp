#include <showcase/editor/CommandHistory.h>

#include <showcase/core/Log.h>

namespace showcase {

// ── CommandHistory ──────────────────────────────────────────────────

void CommandHistory::ExecuteCommand(std::unique_ptr<Command> cmd) {
    cmd->Execute();
    m_undoStack.push_back(std::move(cmd));
    m_redoStack.clear();
    if (m_dirtyCallback)
        m_dirtyCallback();
}

void CommandHistory::RecordCommand(std::unique_ptr<Command> cmd) {
    m_undoStack.push_back(std::move(cmd));
    m_redoStack.clear();
    if (m_dirtyCallback)
        m_dirtyCallback();
}

void CommandHistory::Undo() {
    if (m_undoStack.empty())
        return;
    auto cmd = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    cmd->Undo();
    m_redoStack.push_back(std::move(cmd));
    if (m_dirtyCallback)
        m_dirtyCallback();
}

void CommandHistory::Redo() {
    if (m_redoStack.empty())
        return;
    auto cmd = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    cmd->Redo();
    m_undoStack.push_back(std::move(cmd));
    if (m_dirtyCallback)
        m_dirtyCallback();
}

void CommandHistory::Clear() {
    m_undoStack.clear();
    m_redoStack.clear();
}

std::string CommandHistory::GetUndoName() const {
    if (m_undoStack.empty())
        return {};
    return m_undoStack.back()->GetName();
}

std::string CommandHistory::GetRedoName() const {
    if (m_redoStack.empty())
        return {};
    return m_redoStack.back()->GetName();
}

// ── TransformCommand ────────────────────────────────────────────────

TransformCommand::TransformCommand(const TransformCommandDesc& desc)
    : m_scene(*desc.scene), m_objectId(desc.objectId), m_oldPos(desc.oldPos), m_oldRot(desc.oldRot),
      m_oldScale(desc.oldScale), m_newPos(desc.newPos), m_newRot(desc.newRot), m_newScale(desc.newScale) {}

void TransformCommand::Execute() {
    Apply(m_newPos, m_newRot, m_newScale);
}

void TransformCommand::Undo() {
    Apply(m_oldPos, m_oldRot, m_oldScale);
}

void TransformCommand::Apply(const Vector3& pos, const Vector3& rot, const Vector3& scl) {
    SceneObject* obj = m_scene.FindById(m_objectId);
    if (!obj)
        return;
    obj->position = pos;
    obj->rotation = rot;
    obj->scale = scl;
    obj->RecomputeWorldTransform();
    obj->UpdateAABB();
}

// ── RenameCommand ───────────────────────────────────────────────────

RenameCommand::RenameCommand(Scene& scene, uint32_t objectId, std::string oldName, std::string newName)
    : m_scene(scene), m_objectId(objectId), m_oldName(std::move(oldName)), m_newName(std::move(newName)) {}

void RenameCommand::Execute() {
    SceneObject* obj = m_scene.FindById(m_objectId);
    if (obj)
        obj->name = m_newName;
}

void RenameCommand::Undo() {
    SceneObject* obj = m_scene.FindById(m_objectId);
    if (obj)
        obj->name = m_oldName;
}

// ── ChangeModelCommand ──────────────────────────────────────────────

ChangeModelCommand::ChangeModelCommand(const ChangeModelCommandDesc& desc)
    : m_scene(*desc.scene), m_objectId(desc.objectId), m_oldSource(desc.oldSource), m_newSource(desc.newSource),
      m_resolver(desc.resolver) {}

void ChangeModelCommand::Execute() {
    Apply(m_newSource);
}

void ChangeModelCommand::Undo() {
    Apply(m_oldSource);
}

void ChangeModelCommand::Apply(const std::string& source) {
    SceneObject* obj = m_scene.FindById(m_objectId);
    if (!obj || !obj->modelComp.has_value())
        return;
    obj->modelComp->modelSource = source;
    if (source.empty()) {
        obj->modelComp->model = nullptr;
    } else if (m_resolver) {
        obj->modelComp->model = m_resolver(source);
    }
    obj->UpdateAABB();
}

// ── ChangeBaseColorCommand ──────────────────────────────────────────

ChangeBaseColorCommand::ChangeBaseColorCommand(const ChangeBaseColorCommandDesc& desc)
    : m_scene(*desc.scene), m_objectId(desc.objectId), m_oldColor(desc.oldColor), m_newColor(desc.newColor) {}

void ChangeBaseColorCommand::Execute() {
    Apply(m_newColor);
}

void ChangeBaseColorCommand::Undo() {
    Apply(m_oldColor);
}

void ChangeBaseColorCommand::Apply(const std::optional<Vector4>& color) {
    SceneObject* obj = m_scene.FindById(m_objectId);
    if (!obj || !obj->modelComp.has_value())
        return;
    obj->modelComp->baseColor = color;
}

// ── AddComponentCommand ─────────────────────────────────────────────

AddComponentCommand::AddComponentCommand(const AddComponentCommandDesc& desc)
    : m_scene(*desc.scene), m_objectId(desc.objectId), m_oldComp(desc.oldComp), m_newComp(desc.newComp) {}

void AddComponentCommand::Execute() {
    Apply(m_newComp);
}

void AddComponentCommand::Undo() {
    Apply(m_oldComp);
}

void AddComponentCommand::Apply(const std::optional<ModelComponent>& comp) {
    SceneObject* obj = m_scene.FindById(m_objectId);
    if (!obj)
        return;
    obj->modelComp = comp;
    obj->UpdateAABB();
}

// ── AddLightComponentCommand ────────────────────────────────────────

AddLightComponentCommand::AddLightComponentCommand(const AddLightComponentCommandDesc& desc)
    : m_scene(*desc.scene), m_objectId(desc.objectId), m_oldComp(desc.oldComp), m_newComp(desc.newComp) {}

void AddLightComponentCommand::Execute() {
    Apply(m_newComp);
}

void AddLightComponentCommand::Undo() {
    Apply(m_oldComp);
}

void AddLightComponentCommand::Apply(const std::optional<LightComponent>& comp) {
    SceneObject* obj = m_scene.FindById(m_objectId);
    if (!obj)
        return;
    obj->lightComp = comp;
}

// ── ChangeLightPropertiesCommand ───────────────────────────────────

ChangeLightPropertiesCommand::ChangeLightPropertiesCommand(const ChangeLightPropertiesCommandDesc& desc)
    : m_scene(*desc.scene), m_objectId(desc.objectId), m_oldProps(desc.oldProps), m_newProps(desc.newProps) {}

void ChangeLightPropertiesCommand::Execute() {
    Apply(m_newProps);
}

void ChangeLightPropertiesCommand::Undo() {
    Apply(m_oldProps);
}

void ChangeLightPropertiesCommand::Apply(const LightComponent& props) {
    SceneObject* obj = m_scene.FindById(m_objectId);
    if (!obj || !obj->lightComp.has_value())
        return;
    *obj->lightComp = props;
}

// ── AddObjectCommand ────────────────────────────────────────────────

AddObjectCommand::AddObjectCommand(Scene& scene, int& selectedObjectId, SceneObject snapshot)
    : m_scene(scene), m_selectedObjectId(selectedObjectId), m_snapshot(std::move(snapshot)),
      m_previousSelection(selectedObjectId) {}

void AddObjectCommand::Execute() {
    // First execution: object is already added by the caller, just record selection
    m_selectedObjectId = static_cast<int>(m_snapshot.id);
}

void AddObjectCommand::Undo() {
    m_scene.RemoveObject(m_snapshot.id);
    m_selectedObjectId = m_previousSelection;
}

void AddObjectCommand::Redo() {
    m_scene.InsertObject(SceneObject(m_snapshot), m_scene.GetObjectCount());
    m_selectedObjectId = static_cast<int>(m_snapshot.id);
}

// ── RemoveObjectCommand ─────────────────────────────────────────────

RemoveObjectCommand::RemoveObjectCommand(Scene& scene, int& selectedObjectId, uint32_t objectId)
    : m_scene(scene), m_selectedObjectId(selectedObjectId), m_previousSelection(selectedObjectId) {
    m_index = m_scene.GetObjectIndex(objectId);
    SceneObject* obj = m_scene.FindById(objectId);
    if (obj)
        m_snapshot = *obj;
}

void RemoveObjectCommand::Execute() {
    m_scene.RemoveObject(m_snapshot.id);
    if (m_selectedObjectId == static_cast<int>(m_snapshot.id))
        m_selectedObjectId = -1;
}

void RemoveObjectCommand::Undo() {
    m_scene.InsertObject(SceneObject(m_snapshot), m_index);
    m_selectedObjectId = m_previousSelection;
}

// ── ToggleEnabledCommand ────────────────────────────────────────────

ToggleEnabledCommand::ToggleEnabledCommand(Scene& scene, uint32_t objectId, bool oldEnabled, bool newEnabled)
    : m_scene(scene), m_objectId(objectId), m_oldEnabled(oldEnabled), m_newEnabled(newEnabled) {}

void ToggleEnabledCommand::Execute() {
    SceneObject* obj = m_scene.FindById(m_objectId);
    if (obj)
        obj->enabled = m_newEnabled;
}

void ToggleEnabledCommand::Undo() {
    SceneObject* obj = m_scene.FindById(m_objectId);
    if (obj)
        obj->enabled = m_oldEnabled;
}

} // namespace showcase
