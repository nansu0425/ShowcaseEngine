#include <showcase/core/Log.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <vector>

namespace showcase {

// ── Listener bridge sink ─────────────────────────────────────────

static std::vector<LogListener*> s_listeners;
static std::mutex s_listenerMutex;

static LogLevel ToLogLevel(spdlog::level::level_enum level) {
    switch (level) {
        case spdlog::level::trace:    return LogLevel::Trace;
        case spdlog::level::info:     return LogLevel::Info;
        case spdlog::level::warn:     return LogLevel::Warn;
        case spdlog::level::err:      return LogLevel::Error;
        case spdlog::level::critical: return LogLevel::Critical;
        default:                      return LogLevel::Trace;
    }
}

// Internal spdlog sink that bridges to LogListener interface
class ListenerBridgeSink : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        // Format timestamp
        std::chrono::system_clock::time_point time = msg.time;
        std::time_t tt = std::chrono::system_clock::to_time_t(time);
        std::tm tm{};
        localtime_s(&tm, &tt);
        char timeBuf[16];
        std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);

        LogMessage logMsg;
        logMsg.level = ToLogLevel(msg.level);
        logMsg.timestamp = timeBuf;
        logMsg.message = std::string(msg.payload.data(), msg.payload.size());

        std::lock_guard<std::mutex> lock(s_listenerMutex);
        for (LogListener* listener : s_listeners) {
            listener->OnLogMessage(logMsg);
        }
    }

    void flush_() override {}
};

// ── Log implementation ───────────────────────────────────────────

std::shared_ptr<spdlog::logger> Log::s_logger;

void Log::Init() {
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("[%T] [%^%l%$] %v");

    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("ShowcaseEngine.log", true);
    fileSink->set_pattern("[%Y-%m-%d %T] [%l] %v");

    auto bridgeSink = std::make_shared<ListenerBridgeSink>();
    bridgeSink->set_level(spdlog::level::trace);

    s_logger = std::make_shared<spdlog::logger>("SE",
        spdlog::sinks_init_list{consoleSink, fileSink, bridgeSink});
    s_logger->set_level(spdlog::level::trace);
    s_logger->flush_on(spdlog::level::warn);

    spdlog::set_default_logger(s_logger);
    SE_LOG_INFO("Logger initialized");
}

void Log::AddSink(spdlog::sink_ptr sink) {
    s_logger->sinks().push_back(std::move(sink));
}

void Log::AddListener(LogListener* listener) {
    std::lock_guard<std::mutex> lock(s_listenerMutex);
    s_listeners.push_back(listener);
}

void Log::RemoveListener(LogListener* listener) {
    std::lock_guard<std::mutex> lock(s_listenerMutex);
    s_listeners.erase(std::remove(s_listeners.begin(), s_listeners.end(), listener), s_listeners.end());
}

} // namespace showcase
