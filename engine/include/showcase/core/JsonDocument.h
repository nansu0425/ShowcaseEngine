#pragma once

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>

namespace showcase {

class JsonNode {
public:
    // Navigation
    JsonNode operator[](const char* key);
    JsonNode operator[](const std::string& key);
    JsonNode operator[](int index);
    JsonNode operator[](size_t index);

    // Getters (return default on type mismatch)
    float GetFloat(float defaultVal = 0.0f) const;
    int GetInt(int defaultVal = 0) const;
    bool GetBool(bool defaultVal = false) const;
    std::string GetString(const std::string& defaultVal = "") const;
    long GetLong(long defaultVal = 0) const;

    // Setters
    void Set(float value);
    void Set(int value);
    void Set(bool value);
    void Set(const std::string& value);
    void Set(const char* value);
    void Set(long value);
    void SetFloatArray(std::initializer_list<float> values);

    // Query
    bool Contains(const std::string& key) const;
    bool IsArray() const;
    size_t Size() const;

private:
    friend class JsonDocument;
    explicit JsonNode(void* jsonPtr);
    void* m_json = nullptr;
};

class JsonDocument {
public:
    JsonDocument();
    ~JsonDocument();
    JsonDocument(JsonDocument&& other) noexcept;
    JsonDocument& operator=(JsonDocument&& other) noexcept;

    JsonDocument(const JsonDocument&) = delete;
    JsonDocument& operator=(const JsonDocument&) = delete;

    // File I/O
    [[nodiscard]] bool LoadFromFile(const std::string& filePath);
    [[nodiscard]] bool SaveToFile(const std::string& filePath, int indent = 2) const;

    // Node access
    JsonNode operator[](const char* key);
    JsonNode operator[](const std::string& key);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace showcase
