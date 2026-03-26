#pragma once

#include <showcase/graphics/Scene.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace showcase {

class EditorController;
struct Model;

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

class TransformCommand : public Command {
public:
    TransformCommand(Scene& scene, uint32_t objectId, Vector3 oldPos, Vector3 oldRot, Vector3 oldScale, Vector3 newPos,
                     Vector3 newRot, Vector3 newScale);
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

class ChangeModelCommand : public Command {
public:
    using ResolveModelFn = std::function<Model*(const std::string&)>;
    ChangeModelCommand(Scene& scene, uint32_t objectId, std::string oldSource, std::string newSource,
                       ResolveModelFn resolver);
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

class ChangeBaseColorCommand : public Command {
public:
    ChangeBaseColorCommand(Scene& scene, uint32_t objectId, std::optional<Vector4> oldColor,
                           std::optional<Vector4> newColor);
    void Execute() override;
    void Undo() override;
    std::string GetName() const override { return "Change Base Color"; }

private:
    void Apply(const std::optional<Vector4>& color);
    Scene& m_scene;
    uint32_t m_objectId;
    std::optional<Vector4> m_oldColor, m_newColor;
};

class AddComponentCommand : public Command {
public:
    AddComponentCommand(Scene& scene, uint32_t objectId, std::optional<ModelComponent> oldComp,
                        std::optional<ModelComponent> newComp);
    void Execute() override;
    void Undo() override;
    std::string GetName() const override { return "Add Component"; }

private:
    void Apply(const std::optional<ModelComponent>& comp);
    Scene& m_scene;
    uint32_t m_objectId;
    std::optional<ModelComponent> m_oldComp, m_newComp;
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
