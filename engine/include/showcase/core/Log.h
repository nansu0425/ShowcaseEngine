#pragma once

#include <spdlog/spdlog.h>
#include <memory>

namespace showcase {

class Log {
public:
    static void Init();
    static std::shared_ptr<spdlog::logger>& GetLogger() { return s_logger; }

private:
    static std::shared_ptr<spdlog::logger> s_logger;
};

} // namespace showcase

#define SE_LOG_TRACE(...)    ::showcase::Log::GetLogger()->trace(__VA_ARGS__)
#define SE_LOG_INFO(...)     ::showcase::Log::GetLogger()->info(__VA_ARGS__)
#define SE_LOG_WARN(...)     ::showcase::Log::GetLogger()->warn(__VA_ARGS__)
#define SE_LOG_ERROR(...)    ::showcase::Log::GetLogger()->error(__VA_ARGS__)
#define SE_LOG_CRITICAL(...) ::showcase::Log::GetLogger()->critical(__VA_ARGS__)
