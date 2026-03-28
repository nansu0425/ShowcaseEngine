#pragma once

#include <showcase/graphics/Scene.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace showcase {

class EditorController;
struct Model;

using ResolveModelFn = std::function<Model*(const std::string&)>;

// ── Command base ────────────────────────────────────────────────────

class Command {
public:
    virtual ~Command() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual void Redo() { Execute(); }
    virtual std::string GetName() const = 0;
};

// ── CommandHistory manager ──────────────────────────────────────────

class CommandHistory {
public:
    using DirtyCallback = std::function<void()>;
    void SetDirtyCallback(DirtyCallback cb) { m_dirtyCallback = std::move(cb); }

    void ExecuteCommand(std::unique_ptr<Command> cmd);
    void RecordCommand(std::unique_ptr<Command> cmd);
    void Undo();
    void Redo();

    bool CanUndo() const { return !m_undoStack.empty(); }
    bool CanRedo() const { return !m_redoStack.empty(); }
    void Clear();

    std::string GetUndoName() const;
    std::string GetRedoName() const;

private:
    std::vector<std::unique_ptr<Command>> m_undoStack;
    std::vector<std::unique_ptr<Command>> m_redoStack;
    DirtyCallback m_dirtyCallback;
};

// ── Concrete commands ───────────────────────────────────────────────

struct TransformCommandDesc {
    Scene* scene;
    uint32_t objectId;
    Vector3 oldPos, oldRot, oldScale;
    Vector3 newPos, newRot, newScale;
};

class TransformCommand : public Command {
public:
    explicit TransformCommand(const TransformCommandDesc& desc);
    void Execute() override;
    void Undo() override;
    std::string GetName() const override { return "Transform"; }

private:
    void Apply(const Vector3& pos, const Vector3& rot, const Vector3& scl);
    Scene& m_scene;
    uint32_t m_objectId;
    Vector3 m_oldPos, m_oldRot, m_oldScale;
    Vector3 m_newPos, m_newRot, m_newScale;
};

class RenameCommand : public Command {
public:
    RenameCommand(Scene& scene, uint32_t objectId, std::string oldName, std::string newName);
    void Execute() override;
    void Undo() override;
    std::string GetName() const override { return "Rename"; }

private:
    Scene& m_scene;
    uint32_t m_objectId;
    std::string m_oldName, m_newName;
};

struct ChangeModelCommandDesc {
    Scene* scene;
    uint32_t objectId;
    std::string oldSource;
    std::string newSource;
    ResolveModelFn resolver;
};

class ChangeModelCommand : public Command {
public:
    explicit ChangeModelCommand(const ChangeModelCommandDesc& desc);
    void Execute() override;
    void Undo() override;
    std::string GetName() const override { return "Change Model"; }

private:
    void Apply(const std::string& source);
    Scene& m_scene;
    uint32_t m_objectId;
    std::string m_oldSource, m_newSource;
    ResolveModelFn m_resolver;
};

struct ChangeBaseColorCommandDesc {
    Scene* scene;
    uint32_t objectId;
    std::optional<Vector4> oldColor;
    std::optional<Vector4> newColor;
};

class ChangeBaseColorCommand : public Command {
public:
    explicit ChangeBaseColorCommand(const ChangeBaseColorCommandDesc& desc);
    void Execute() override;
    void Undo() override;
    std::string GetName() const override { return "Change Base Color"; }

private:
    void Apply(const std::optional<Vector4>& color);
    Scene& m_scene;
    uint32_t m_objectId;
    std::optional<Vector4> m_oldColor, m_newColor;
};

struct AddComponentCommandDesc {
    Scene* scene;
    uint32_t objectId;
    std::optional<ModelComponent> oldComp;
    std::optional<ModelComponent> newComp;
};

class AddComponentCommand : public Command {
public:
    explicit AddComponentCommand(const AddComponentCommandDesc& desc);
    void Execute() override;
    void Undo() override;
    std::string GetName() const override { return "Add Component"; }

private:
    void Apply(const std::optional<ModelComponent>& comp);
    Scene& m_scene;
    uint32_t m_objectId;
    std::optional<ModelComponent> m_oldComp, m_newComp;
};

struct AddLightComponentCommandDesc {
    Scene* scene;
    uint32_t objectId;
    std::optional<LightComponent> oldComp;
    std::optional<LightComponent> newComp;
};

class AddLightComponentCommand : public Command {
public:
    explicit AddLightComponentCommand(const AddLightComponentCommandDesc& desc);
    void Execute() override;
    void Undo() override;
    std::string GetName() const override { return "Light Component"; }

private:
    void Apply(const std::optional<LightComponent>& comp);
    Scene& m_scene;
    uint32_t m_objectId;
    std::optional<LightComponent> m_oldComp, m_newComp;
};

struct ChangeLightPropertiesCommandDesc {
    Scene* scene;
    uint32_t objectId;
    LightComponent oldProps;
    LightComponent newProps;
};

class ChangeLightPropertiesCommand : public Command {
public:
    explicit ChangeLightPropertiesCommand(const ChangeLightPropertiesCommandDesc& desc);
    void Execute() override;
    void Undo() override;
    std::string GetName() const override { return "Change Light"; }

private:
    void Apply(const LightComponent& props);
    Scene& m_scene;
    uint32_t m_objectId;
    LightComponent m_oldProps, m_newProps;
};

class AddObjectCommand : public Command {
public:
    AddObjectCommand(Scene& scene, int& selectedObjectId, SceneObject snapshot);
    void Execute() override;
    void Undo() override;
    void Redo() override;
    std::string GetName() const override { return "Add Object"; }

private:
    Scene& m_scene;
    int& m_selectedObjectId;
    SceneObject m_snapshot;
    int m_previousSelection;
};

class ToggleEnabledCommand : public Command {
public:
    ToggleEnabledCommand(Scene& scene, uint32_t objectId, bool oldEnabled, bool newEnabled);
    void Execute() override;
    void Undo() override;
    std::string GetName() const override { return "Toggle Enabled"; }

private:
    Scene& m_scene;
    uint32_t m_objectId;
    bool m_oldEnabled, m_newEnabled;
};

class RemoveObjectCommand : public Command {
public:
    RemoveObjectCommand(Scene& scene, int& selectedObjectId, uint32_t objectId);
    void Execute() override;
    void Undo() override;
    std::string GetName() const override { return "Remove Object"; }

private:
    Scene& m_scene;
    int& m_selectedObjectId;
    SceneObject m_snapshot;
    size_t m_index = 0;
    int m_previousSelection;
};

} // namespace showcase
