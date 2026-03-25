#include <showcase/core/JsonDocument.h>

#include <showcase/core/Log.h>
#include <showcase/core/Profiler.h>

#include <nlohmann/json.hpp>

#include <fstream>

namespace showcase {

// ── JsonNode ─────────────────────────────────────────────────────

JsonNode::JsonNode(void* jsonPtr) : m_json(jsonPtr) {}

JsonNode JsonNode::operator[](const char* key) {
    auto* j = static_cast<nlohmann::json*>(m_json);
    return JsonNode(&(*j)[key]);
}

JsonNode JsonNode::operator[](const std::string& key) {
    return operator[](key.c_str());
}

JsonNode JsonNode::operator[](int index) {
    return operator[](static_cast<size_t>(index));
}

JsonNode JsonNode::operator[](size_t index) {
    auto* j = static_cast<nlohmann::json*>(m_json);
    return JsonNode(&(*j)[index]);
}

float JsonNode::GetFloat(float defaultVal) const {
    auto* j = static_cast<nlohmann::json*>(m_json);
    if (j && j->is_number()) {
        return j->get<float>();
    }
    return defaultVal;
}

int JsonNode::GetInt(int defaultVal) const {
    auto* j = static_cast<nlohmann::json*>(m_json);
    if (j && j->is_number()) {
        return j->get<int>();
    }
    return defaultVal;
}

bool JsonNode::GetBool(bool defaultVal) const {
    auto* j = static_cast<nlohmann::json*>(m_json);
    if (j && j->is_boolean()) {
        return j->get<bool>();
    }
    return defaultVal;
}

std::string JsonNode::GetString(const std::string& defaultVal) const {
    auto* j = static_cast<nlohmann::json*>(m_json);
    if (j && j->is_string()) {
        return j->get<std::string>();
    }
    return defaultVal;
}

long JsonNode::GetLong(long defaultVal) const {
    auto* j = static_cast<nlohmann::json*>(m_json);
    if (j && j->is_number()) {
        return j->get<long>();
    }
    return defaultVal;
}

void JsonNode::Set(float value) {
    auto* j = static_cast<nlohmann::json*>(m_json);
    *j = value;
}

void JsonNode::Set(int value) {
    auto* j = static_cast<nlohmann::json*>(m_json);
    *j = value;
}

void JsonNode::Set(bool value) {
    auto* j = static_cast<nlohmann::json*>(m_json);
    *j = value;
}

void JsonNode::Set(const std::string& value) {
    auto* j = static_cast<nlohmann::json*>(m_json);
    *j = value;
}

void JsonNode::Set(const char* value) {
    auto* j = static_cast<nlohmann::json*>(m_json);
    *j = value;
}

void JsonNode::Set(long value) {
    auto* j = static_cast<nlohmann::json*>(m_json);
    *j = value;
}

void JsonNode::SetFloatArray(std::initializer_list<float> values) {
    auto* j = static_cast<nlohmann::json*>(m_json);
    *j = nlohmann::json::array();
    for (float v : values) {
        j->push_back(v);
    }
}

void JsonNode::SetArray() {
    auto* j = static_cast<nlohmann::json*>(m_json);
    *j = nlohmann::json::array();
}

JsonNode JsonNode::PushBack() {
    auto* j = static_cast<nlohmann::json*>(m_json);
    j->push_back(nlohmann::json::object());
    return JsonNode(&j->back());
}

bool JsonNode::Contains(const std::string& key) const {
    auto* j = static_cast<nlohmann::json*>(m_json);
    if (j && j->is_object()) {
        return j->contains(key);
    }
    return false;
}

bool JsonNode::IsArray() const {
    auto* j = static_cast<nlohmann::json*>(m_json);
    return j && j->is_array();
}

size_t JsonNode::Size() const {
    auto* j = static_cast<nlohmann::json*>(m_json);
    if (j && (j->is_array() || j->is_object())) {
        return j->size();
    }
    return 0;
}

// ── JsonDocument ─────────────────────────────────────────────────

struct JsonDocument::Impl {
    nlohmann::json data;
};

JsonDocument::JsonDocument() : m_impl(std::make_unique<Impl>()) {
    m_impl->data = nlohmann::json::object();
}

JsonDocument::~JsonDocument() = default;

JsonDocument::JsonDocument(JsonDocument&& other) noexcept = default;

JsonDocument& JsonDocument::operator=(JsonDocument&& other) noexcept = default;

bool JsonDocument::LoadFromFile(const std::string& filePath) {
    SE_ZONE_SCOPED_C(profile::kColorAssetIO);
    SE_ZONE_TEXT(filePath.c_str(), filePath.size());
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    if (!nlohmann::json::accept(content)) {
        SE_LOG_WARN("Failed to parse JSON: {}", filePath);
        return false;
    }

    m_impl->data = nlohmann::json::parse(content);
    return true;
}

bool JsonDocument::SaveToFile(const std::string& filePath, int indent) const {
    SE_ZONE_SCOPED_C(profile::kColorAssetIO);
    SE_ZONE_TEXT(filePath.c_str(), filePath.size());
    std::ofstream file(filePath);
    if (!file.is_open()) {
        SE_LOG_WARN("Failed to open file for writing: {}", filePath);
        return false;
    }

    file << m_impl->data.dump(indent);
    return true;
}

JsonNode JsonDocument::operator[](const char* key) {
    return JsonNode(&m_impl->data[key]);
}

JsonNode JsonDocument::operator[](const std::string& key) {
    return operator[](key.c_str());
}

} // namespace showcase
