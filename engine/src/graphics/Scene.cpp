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
    if (HasModel()) {
        modelComp->model->localAABB.Transform(worldAABB, worldTransform);
    }
}

// ── Scene management ─────────────────────────────────────────────────
SceneObject& Scene::AddObject(Model* model, const Matrix& transform) {
    SceneObject obj;
    obj.id = m_nextId++;
    obj.worldTransform = transform;
    if (model) {
        obj.modelComp = ModelComponent{"", model};
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
        obj.modelComp = ModelComponent{"", model};
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
    doc["version"].Set(3);

    auto objects = doc["objects"];
    objects.SetArray();

    for (const auto& obj : m_objects) {
        auto node = objects.PushBack();
        node["name"].Set(obj.name);
        node["position"].SetFloatArray({obj.position.x, obj.position.y, obj.position.z});
        node["rotation"].SetFloatArray({obj.rotation.x, obj.rotation.y, obj.rotation.z});
        node["scale"].SetFloatArray({obj.scale.x, obj.scale.y, obj.scale.z});

        if (obj.modelComp.has_value()) {
            auto model = node["components"]["model"];
            model["mesh"].Set(obj.modelComp->modelSource);

            if (obj.modelComp->baseColor.has_value()) {
                const auto& c = *obj.modelComp->baseColor;
                model["material"]["baseColor"].SetFloatArray({c.x, c.y, c.z, c.w});
            }
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

        // v3: components.model.mesh + components.model.material
        // v2: components.mesh.modelSource
        // v1: top-level modelSource
        auto components = node["components"];
        auto modelNode = components["model"];

        if (modelNode.Contains("mesh")) {
            // v3 format
            std::string modelSource = modelNode["mesh"].GetString();
            if (!modelSource.empty()) {
                obj.modelComp = ModelComponent{modelSource, nullptr};
            }

            auto matNode = modelNode["material"];
            if (obj.modelComp.has_value() && matNode.Contains("baseColor")) {
                auto bc = matNode["baseColor"];
                if (bc.IsArray() && bc.Size() >= 4) {
                    obj.modelComp->baseColor =
                        Vector4(bc[0].GetFloat(), bc[1].GetFloat(), bc[2].GetFloat(), bc[3].GetFloat());
                }
            }
        } else if (version >= 2) {
            // v2 legacy: components.mesh.modelSource
            auto mesh = components["mesh"];
            if (mesh.Contains("modelSource")) {
                std::string modelSource = mesh["modelSource"].GetString();
                if (!modelSource.empty()) {
                    obj.modelComp = ModelComponent{modelSource, nullptr};
                }
            }
        } else {
            // v1 legacy: top-level modelSource
            std::string modelSource = node["modelSource"].GetString();
            if (!modelSource.empty()) {
                obj.modelComp = ModelComponent{modelSource, nullptr};
            }
        }

        // Legacy fallback: top-level baseColorOverride → ModelComponent::baseColor
        if (obj.modelComp.has_value() && !obj.modelComp->baseColor.has_value()) {
            auto colorOverride = node["baseColorOverride"];
            if (colorOverride.IsArray() && colorOverride.Size() >= 4) {
                obj.modelComp->baseColor = Vector4(colorOverride[0].GetFloat(), colorOverride[1].GetFloat(),
                                                   colorOverride[2].GetFloat(), colorOverride[3].GetFloat());
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

SceneObject& Scene::InsertObject(SceneObject obj, size_t index) {
    if (obj.id >= m_nextId) {
        m_nextId = obj.id + 1;
    }
    index = std::min(index, m_objects.size());
    auto it = m_objects.insert(m_objects.begin() + static_cast<ptrdiff_t>(index), std::move(obj));
    return *it;
}

size_t Scene::GetObjectIndex(uint32_t id) const {
    for (size_t i = 0; i < m_objects.size(); ++i) {
        if (m_objects[i].id == id)
            return i;
    }
    return m_objects.size();
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
