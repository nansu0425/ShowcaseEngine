#include <showcase/graphics/Scene.h>

namespace showcase {

bool SceneObject::IsVisible() const {
    return !frustumCulled && !occlusionCulled;
}

SceneObject& Scene::AddObject(Model* model, const DirectX::SimpleMath::Matrix& transform) {
    SceneObject obj;
    obj.model = model;
    obj.worldTransform = transform;
    model->localAABB.Transform(obj.worldAABB, transform);
    m_objects.push_back(std::move(obj));
    return m_objects.back();
}

void Scene::Clear() {
    m_objects.clear();
}

std::vector<SceneObject>& Scene::GetObjects() {
    return m_objects;
}

const std::vector<SceneObject>& Scene::GetObjects() const {
    return m_objects;
}

size_t Scene::GetObjectCount() const {
    return m_objects.size();
}

} // namespace showcase
