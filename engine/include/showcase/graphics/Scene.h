#pragma once

#include <showcase/core/JsonDocument.h>
#include <showcase/graphics/Model.h>

#include <optional>
#include <string>
#include <vector>

namespace showcase {

// ── Light ───────────────────────────────────────────────────────────

enum class LightType : int {
    Directional = 0,
    Ambient = 1,
    Point = 2,
    Spot = 3,
};

struct LightComponent {
    LightType type = LightType::Directional;
    Vector3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float specularPower = 32.0f;
    float range = 10.0f;      // Point/Spot — radius of influence (meters)
    float innerAngle = 15.0f; // Spot only — full intensity cone half-angle (degrees)
    float outerAngle = 30.0f; // Spot only — falloff-to-zero cone half-angle (degrees)
    bool castShadow = true;
    float shadowBias = 0.001f; // Shader-side depth bias
    bool enablePCF = true;     // true = 3x3 PCF (soft), false = single sample (hard)
};

struct DirectionalLightData {
    Vector3 direction;
    Vector3 color;
    float specularPower;
    bool castShadow;
    float shadowBias;
    bool enablePCF;
};

struct AmbientLightData {
    Vector3 color; // color * intensity
};

struct PointLightData {
    Vector3 position;
    float range;
    Vector3 color; // color * intensity
    float specularPower;
    bool castShadow;
    float shadowBias;
    bool enablePCF;
    int objectId = -1;
};

struct SpotLightData {
    Vector3 position;
    float range;
    Vector3 direction;
    float innerCos; // cos(innerAngle)
    Vector3 color;  // color * intensity
    float outerCos; // cos(outerAngle)
    float specularPower;
    bool castShadow;
    float shadowBias;
    bool enablePCF;
    float outerAngle; // radians — needed for shadow frustum FOV
    int objectId = -1;
};

// ── Components ──────────────────────────────────────────────────────

struct ModelComponent {
    std::string modelSource;          // e.g. "builtin:cube", "file:assets/models/duck.glb"
    Model* model = nullptr;           // non-owning, resolved at runtime
    std::optional<Vector4> baseColor; // instance-level material override
};

struct SceneObject {
    uint32_t id = 0;
    std::string name;
    bool enabled = true;
    Vector3 position = {};
    Vector3 rotation = {}; // Euler degrees (Y→X→Z)
    Vector3 scale = {1, 1, 1};
    Matrix worldTransform;
    BoundingBox worldAABB;
    bool frustumCulled = false;
    bool occlusionCulled = false;
    int lodLevel = 0;

    std::optional<ModelComponent> modelComp;
    std::optional<LightComponent> lightComp;

    bool HasModel() const { return modelComp.has_value() && modelComp->model != nullptr; }
    bool HasLight() const { return lightComp.has_value(); }
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
    uint32_t AllocateId() { return m_nextId++; }
    std::string GenerateUniqueName(const std::string& baseName) const;
    size_t GetObjectCount() const;
    size_t GetObjectIndex(uint32_t id) const;
    std::optional<DirectionalLightData> GetDirectionalLight() const;
    std::optional<AmbientLightData> GetAmbientLight() const;
    std::vector<PointLightData> GetPointLights() const;
    std::vector<SpotLightData> GetSpotLights() const;
    std::optional<BoundingBox> GetSceneAABB() const;

private:
    std::vector<SceneObject> m_objects;
    uint32_t m_nextId = 1;
};

} // namespace showcase
