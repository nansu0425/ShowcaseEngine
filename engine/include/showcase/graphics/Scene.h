#pragma once

#include <showcase/core/JsonDocument.h>
#include <showcase/graphics/Model.h>

#include <optional>
#include <string>
#include <vector>

namespace showcase {

struct ModelComponent {
    std::string modelSource;          // e.g. "builtin:cube", "file:assets/models/duck.glb"
    Model* model = nullptr;           // non-owning, resolved at runtime
    std::optional<Vector4> baseColor; // instance-level material override
};

struct SceneObject {
    uint32_t id = 0;
    std::string name;
    Vector3 position = {};
    Vector3 rotation = {}; // Euler degrees (Y→X→Z)
    Vector3 scale = {1, 1, 1};
    Matrix worldTransform;
    BoundingBox worldAABB;
    bool frustumCulled = false;
    bool occlusionCulled = false;
    int lodLevel = 0;

    std::optional<ModelComponent> modelComp;

    bool HasModel() const { return modelComp.has_value() && modelComp->model != nullptr; }
    bool IsVisible() const;
    void RecomputeWorldTransform();
    void UpdateAABB();
};

struct AddObjectDesc {
    Model* model = nullptr;
    std::string name;
    Vector3 position = {};
    Vector3 rotation = {};
    Vector3 scale = {1, 1, 1};
};

class Scene {
public:
    SceneObject& AddObject(Model* model, const Matrix& transform);
    SceneObject& AddObject(const AddObjectDesc& desc);
    bool RemoveObject(uint32_t id);
    void Clear();

    void Serialize(JsonDocument& doc) const;
    bool Deserialize(JsonDocument& doc);

    [[nodiscard]] bool SaveToFile(const std::string& filePath) const;
    [[nodiscard]] bool LoadFromFile(const std::string& filePath);

    SceneObject* FindById(uint32_t id);
    SceneObject& InsertObject(SceneObject obj, size_t index);
    std::vector<SceneObject>& GetObjects();
    const std::vector<SceneObject>& GetObjects() const;
    size_t GetObjectCount() const;
    size_t GetObjectIndex(uint32_t id) const;

private:
    std::vector<SceneObject> m_objects;
    uint32_t m_nextId = 1;
};

} // namespace showcase
