#pragma once

#include <showcase/graphics/Model.h>
#include <SimpleMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <string>

namespace showcase {

struct SceneObject {
    uint32_t id = 0;
    std::string name;
    Model* model = nullptr;
    DirectX::SimpleMath::Vector3 position = {};
    DirectX::SimpleMath::Vector3 rotation = {};       // Euler degrees (Y→X→Z)
    DirectX::SimpleMath::Vector3 scale = {1, 1, 1};
    DirectX::SimpleMath::Matrix worldTransform;
    DirectX::BoundingBox worldAABB;
    bool frustumCulled = false;
    bool occlusionCulled = false;
    int lodLevel = 0;

    bool IsVisible() const;
    void RecomputeWorldTransform();
    void UpdateAABB();
};

class Scene {
public:
    SceneObject& AddObject(Model* model, const DirectX::SimpleMath::Matrix& transform);
    SceneObject& AddObject(Model* model, const std::string& name,
                           const DirectX::SimpleMath::Vector3& pos,
                           const DirectX::SimpleMath::Vector3& rot = {},
                           const DirectX::SimpleMath::Vector3& scl = {1, 1, 1});
    void Clear();

    SceneObject* FindById(uint32_t id);
    std::vector<SceneObject>& GetObjects();
    const std::vector<SceneObject>& GetObjects() const;
    size_t GetObjectCount() const;

private:
    std::vector<SceneObject> m_objects;
    uint32_t m_nextId = 1;
};

} // namespace showcase
