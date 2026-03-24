#include <showcase/graphics/Scene.h>

#include <showcase/core/JsonDocument.h>
#include <showcase/core/Log.h>

#include <algorithm>

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

    worldTransform = Matrix::CreateScale(safeScale) * Matrix::CreateRotationZ(rz) * Matrix::CreateRotationX(rx) *
                     Matrix::CreateRotationY(ry) * Matrix::CreateTranslation(position);
}

void SceneObject::UpdateAABB() {
    if (HasMesh()) {
        mesh->model->localAABB.Transform(worldAABB, worldTransform);
    }
}

// ── Scene management ─────────────────────────────────────────────────
SceneObject& Scene::AddObject(Model* model, const Matrix& transform) {
    SceneObject obj;
    obj.id = m_nextId++;
    obj.worldTransform = transform;
    if (model) {
        obj.mesh = MeshComponent{"", model};
        model->localAABB.Transform(obj.worldAABB, transform);
    }
    m_objects.push_back(std::move(obj));
    return m_objects.back();
}

SceneObject& Scene::AddObject(Model* model, const std::string& name, const Vector3& pos, const Vector3& rot,
                              const Vector3& scl) {
    SceneObject obj;
    obj.id = m_nextId++;
    obj.name = name;
    if (model) {
        obj.mesh = MeshComponent{"", model};
    }
    obj.position = pos;
    obj.rotation = rot;
    obj.scale = scl;
    obj.RecomputeWorldTransform();
    obj.UpdateAABB();
    m_objects.push_back(std::move(obj));
    return m_objects.back();
}

bool Scene::RemoveObject(uint32_t id) {
    auto it = std::find_if(m_objects.begin(), m_objects.end(), [id](const SceneObject& obj) { return obj.id == id; });
    if (it == m_objects.end())
        return false;
    m_objects.erase(it);
    return true;
}

// ── Query ────────────────────────────────────────────────────────────
SceneObject* Scene::FindById(uint32_t id) {
    for (auto& obj : m_objects) {
        if (obj.id == id)
            return &obj;
    }
    return nullptr;
}

void Scene::Clear() {
    m_objects.clear();
    m_nextId = 1;
}

// ── Serialization ───────────────────────────────────────────────────
void Scene::Serialize(JsonDocument& doc) const {
    doc["version"].Set(2);

    auto objects = doc["objects"];
    objects.SetArray();

    for (const auto& obj : m_objects) {
        auto node = objects.PushBack();
        node["name"].Set(obj.name);
        node["position"].SetFloatArray({obj.position.x, obj.position.y, obj.position.z});
        node["rotation"].SetFloatArray({obj.rotation.x, obj.rotation.y, obj.rotation.z});
        node["scale"].SetFloatArray({obj.scale.x, obj.scale.y, obj.scale.z});

        if (obj.mesh.has_value()) {
            auto components = node["components"];
            auto mesh = components["mesh"];
            mesh["modelSource"].Set(obj.mesh->modelSource);
        }
    }
}

bool Scene::Deserialize(JsonDocument& doc) {
    m_objects.clear();
    m_nextId = 1;

    // GetInt returns 0 if key missing; version was always 1 or 2
    int version = doc["version"].GetInt();
    if (version < 1)
        version = 1;

    auto objects = doc["objects"];
    if (!objects.IsArray()) {
        return false;
    }

    for (size_t i = 0; i < objects.Size(); ++i) {
        auto node = objects[i];
        SceneObject obj;
        obj.id = m_nextId++;
        obj.name = node["name"].GetString();

        // v2: components.mesh.modelSource, v1: top-level modelSource
        if (version >= 2) {
            auto components = node["components"];
            auto mesh = components["mesh"];
            if (mesh.Contains("modelSource")) {
                std::string modelSource = mesh["modelSource"].GetString();
                if (!modelSource.empty()) {
                    obj.mesh = MeshComponent{modelSource, nullptr};
                }
            }
        } else {
            std::string modelSource = node["modelSource"].GetString();
            if (!modelSource.empty()) {
                obj.mesh = MeshComponent{modelSource, nullptr};
            }
        }

        auto pos = node["position"];
        if (pos.IsArray() && pos.Size() >= 3) {
            obj.position = Vector3(pos[0].GetFloat(), pos[1].GetFloat(), pos[2].GetFloat());
        }

        auto rot = node["rotation"];
        if (rot.IsArray() && rot.Size() >= 3) {
            obj.rotation = Vector3(rot[0].GetFloat(), rot[1].GetFloat(), rot[2].GetFloat());
        }

        auto scl = node["scale"];
        if (scl.IsArray() && scl.Size() >= 3) {
            obj.scale = Vector3(scl[0].GetFloat(), scl[1].GetFloat(), scl[2].GetFloat());
        }

        m_objects.push_back(std::move(obj));
    }

    return true;
}

bool Scene::SaveToFile(const std::string& filePath) const {
    JsonDocument doc;
    Serialize(doc);

    if (!doc.SaveToFile(filePath)) {
        SE_LOG_ERROR("Failed to save scene: {}", filePath);
        return false;
    }

    SE_LOG_INFO("Scene saved: {}", filePath);
    return true;
}

bool Scene::LoadFromFile(const std::string& filePath) {
    JsonDocument doc;
    if (!doc.LoadFromFile(filePath)) {
        SE_LOG_ERROR("Failed to load scene: {}", filePath);
        return false;
    }

    if (!Deserialize(doc)) {
        SE_LOG_ERROR("Scene file missing 'objects' array: {}", filePath);
        return false;
    }

    SE_LOG_INFO("Scene loaded: {} ({} objects)", filePath, m_objects.size());
    return true;
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
