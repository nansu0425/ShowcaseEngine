#pragma once

#include <showcase/graphics/Model.h>
#include <showcase/core/JsonDocument.h>

#include <vector>
#include <string>

namespace showcase {

struct SceneObject {
    uint32_t id = 0;
    std::string name;
    std::string modelSource;   // e.g. "builtin:cube", "file:assets/models/duck.gltf"
    Model* model = nullptr;
    Vector3 position = {};
    Vector3 rotation = {};       // Euler degrees (Y→X→Z)
    Vector3 scale = {1, 1, 1};
    Matrix worldTransform;
    BoundingBox worldAABB;
    bool frustumCulled = false;
    bool occlusionCulled = false;
    int lodLevel = 0;

    bool IsVisible() const;
    void RecomputeWorldTransform();
    void UpdateAABB();
};

class Scene {
public:
    SceneObject& AddObject(Model* model, const Matrix& transform);
    SceneObject& AddObject(Model* model, const std::string& name,
                           const Vector3& pos,
                           const Vector3& rot = {},
                           const Vector3& scl = {1, 1, 1});
    void Clear();

    void Serialize(JsonDocument& doc) const;
    bool Deserialize(JsonDocument& doc);

    [[nodiscard]] bool SaveToFile(const std::string& filePath) const;
    [[nodiscard]] bool LoadFromFile(const std::string& filePath);

    SceneObject* FindById(uint32_t id);
    std::vector<SceneObject>& GetObjects();
    const std::vector<SceneObject>& GetObjects() const;
    size_t GetObjectCount() const;

private:
    std::vector<SceneObject> m_objects;
    uint32_t m_nextId = 1;
};

} // namespace showcase
