#include "LambUI/UILog.h"

#include <cstdio>

namespace LambUI {

LogCallback Log::s_callback;
LogLevel Log::s_minLevel = LogLevel::Trace;

const char* ToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

void Log::SetCallback(LogCallback callback) {
    s_callback = std::move(callback);
}

void Log::SetMinLevel(LogLevel level) {
    s_minLevel = level;
}

void Log::Write(LogLevel level, const char* tag, const std::string& message) {
    if (!s_callback || level < s_minLevel) return;
    s_callback(level, tag, message);
}

void Log::UseDefaultConsoleSink() {
    SetCallback([](LogLevel level, const char* tag, const std::string& message) {
        std::FILE* stream = (level == LogLevel::Warn || level == LogLevel::Error) ? stderr : stdout;
        std::fprintf(stream, "[%s] %s: %s\n", ToString(level), tag, message.c_str());
    });
}

} // namespace LambUI
