#include <showcase/core/Log.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace showcase {

std::shared_ptr<spdlog::logger> Log::s_logger;

void Log::Init() {
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("[%T] [%^%l%$] %v");

    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("ShowcaseEngine.log", true);
    fileSink->set_pattern("[%Y-%m-%d %T] [%l] %v");

    s_logger = std::make_shared<spdlog::logger>("SE",
        spdlog::sinks_init_list{consoleSink, fileSink});
    s_logger->set_level(spdlog::level::trace);
    s_logger->flush_on(spdlog::level::warn);

    spdlog::set_default_logger(s_logger);
    SE_LOG_INFO("Logger initialized");
}

} // namespace showcase
