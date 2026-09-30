#pragma once

#include "lambui_export.h"
#include <fmt/format.h>
#include <functional>
#include <string>
#include <utility>

namespace LambUI {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error
};

const char* ToString(LogLevel level);

// Android-style (tag, message) log sink. Consumers register one callback to
// forward all of LambUI's internal logging to whatever system their engine
// already uses (printf/cout, spdlog, an in-game console, etc). With no sink
// registered, logging calls are simply no-ops.
using LogCallback = std::function<void(LogLevel level, const char* tag, const std::string& message)>;

// Process-wide logging facility. The library never picks a logging backend
// for you; it only ever calls into the callback the host application supplies.
class LAMBUI_API Log {
public:
    static void SetCallback(LogCallback callback);
    // Messages below this level are dropped before reaching the callback.
    static void SetMinLevel(LogLevel level);

    static void Write(LogLevel level, const char* tag, const std::string& message);

    // {fmt}-based convenience, with compile-time format string checking.
    template <typename... Args>
    static void Writef(LogLevel level, const char* tag, fmt::format_string<Args...> format, Args&&... args) {
        Write(level, tag, fmt::format(format, std::forward<Args>(args)...));
    }

    // Zero-setup option for hosts that just want console output; prints
    // "[LEVEL] tag: message" to stdout (stderr for Warn/Error).
    static void UseDefaultConsoleSink();

private:
    static LogCallback s_callback;
    static LogLevel s_minLevel;
};

} // namespace LambUI

// Convenience macros mirroring Android's Log.v/d/i/w/e, e.g.
// LAMBUI_LOGD("UIManager", "widget {} created", widget->GetName()).
#define LAMBUI_LOGT(tag, format, ...) ::LambUI::Log::Writef(::LambUI::LogLevel::Trace, tag, format, ##__VA_ARGS__)
#define LAMBUI_LOGD(tag, format, ...) ::LambUI::Log::Writef(::LambUI::LogLevel::Debug, tag, format, ##__VA_ARGS__)
#define LAMBUI_LOGI(tag, format, ...) ::LambUI::Log::Writef(::LambUI::LogLevel::Info, tag, format, ##__VA_ARGS__)
#define LAMBUI_LOGW(tag, format, ...) ::LambUI::Log::Writef(::LambUI::LogLevel::Warn, tag, format, ##__VA_ARGS__)
#define LAMBUI_LOGE(tag, format, ...) ::LambUI::Log::Writef(::LambUI::LogLevel::Error, tag, format, ##__VA_ARGS__)
