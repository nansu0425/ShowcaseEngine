#include <showcase/graphics/Scene.h>

namespace showcase {

// ── SceneObject ──────────────────────────────────────────────────────
bool SceneObject::IsVisible() const {
    return !frustumCulled && !occlusionCulled;
}

void SceneObject::RecomputeWorldTransform() {
    Vector3 safeScale = ClampScale(scale);

    float rx = ToRadians(rotation.x);
    float ry = ToRadians(rotation.y);
    float rz = ToRadians(rotation.z);

    worldTransform = Matrix::CreateScale(safeScale)
                   * Matrix::CreateRotationZ(rz)
                   * Matrix::CreateRotationX(rx)
                   * Matrix::CreateRotationY(ry)
                   * Matrix::CreateTranslation(position);
}

void SceneObject::UpdateAABB() {
    if (model) {
        model->localAABB.Transform(worldAABB, worldTransform);
    }
}

// ── Scene management ─────────────────────────────────────────────────
SceneObject& Scene::AddObject(Model* model, const Matrix& transform) {
    SceneObject obj;
    obj.id = m_nextId++;
    obj.model = model;
    obj.worldTransform = transform;
    if (model) {
        model->localAABB.Transform(obj.worldAABB, transform);
    }
    m_objects.push_back(std::move(obj));
    return m_objects.back();
}

SceneObject& Scene::AddObject(Model* model, const std::string& name,
                               const Vector3& pos, const Vector3& rot, const Vector3& scl) {
    SceneObject obj;
    obj.id = m_nextId++;
    obj.name = name;
    obj.model = model;
    obj.position = pos;
    obj.rotation = rot;
    obj.scale = scl;
    obj.RecomputeWorldTransform();
    obj.UpdateAABB();
    m_objects.push_back(std::move(obj));
    return m_objects.back();
}

// ── Query ────────────────────────────────────────────────────────────
SceneObject* Scene::FindById(uint32_t id) {
    for (auto& obj : m_objects) {
        if (obj.id == id) return &obj;
    }
    return nullptr;
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
