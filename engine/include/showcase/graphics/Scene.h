#pragma once

#include <showcase/graphics/Model.h>
#include <SimpleMath.h>
#include <DirectXCollision.h>
#include <vector>

namespace showcase {

struct SceneObject {
    Model* model = nullptr;
    DirectX::SimpleMath::Matrix worldTransform;
    DirectX::BoundingBox worldAABB;
    bool frustumCulled = false;
    bool occlusionCulled = false;
    int lodLevel = 0;

    bool IsVisible() const;
};

class Scene {
public:
    SceneObject& AddObject(Model* model, const DirectX::SimpleMath::Matrix& transform);
    void Clear();

    std::vector<SceneObject>& GetObjects();
    const std::vector<SceneObject>& GetObjects() const;
    size_t GetObjectCount() const;

private:
    std::vector<SceneObject> m_objects;
};

} // namespace showcase
