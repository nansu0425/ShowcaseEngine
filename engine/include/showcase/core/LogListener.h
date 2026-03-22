#pragma once

#include <string>
#include <cstdint>

namespace showcase {

enum class LogLevel : uint8_t {
    Trace,
    Info,
    Warn,
    Error,
    Critical
};

struct LogMessage {
    LogLevel level;
    std::string timestamp; // Pre-formatted "HH:MM:SS"
    std::string message;
};

class LogListener {
public:
    virtual ~LogListener() = default;
    virtual void OnLogMessage(const LogMessage& msg) = 0;
};

} // namespace showcase
