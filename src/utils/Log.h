#pragma once

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <vector>
#include <algorithm>

// ============================================================
// 日志系统 - 注册模式的模块化结构
// ============================================================
// 使用方式：
//   1. 在模块 cpp 中创建静态 LogSource：static LogSource log("模块名");
//   2. 使用 log.Info("...") / log.Warn("...") / log.Error("...")
//   3. 也可使用全局 Log() / LogWarn() 等兼容函数
// ============================================================

// 日志级别（从低到高）
enum class LogLevel
{
    Debug,
    Info,
    Warn,
    Error
};

// 日志回调函数类型
using LogCallback = void (*)(LogLevel level, const char *module, const char *msg);

// ============================================================
// LogSource: 模块化日志源，每个模块注册一个实例
// ============================================================
class LogSource
{
public:
    LogSource(const char *moduleName);
    ~LogSource();

    const char *GetModuleName() const { return m_ModuleName; }

    void Debug(const char *fmt, ...) const;
    void Info(const char *fmt, ...) const;
    void Warn(const char *fmt, ...) const;
    void Error(const char *fmt, ...) const;

    static void SetCallback(LogCallback callback);
    static void SetMinLevel(LogLevel level);
    static LogLevel GetMinLevel();
    static void SetEnabled(bool enabled);
    static bool IsEnabled();
    static const std::vector<LogSource *> &GetAll();

private:
    const char *m_ModuleName;

    // 全局状态
    static LogCallback &GetCallback();
    static LogLevel &GetMinLevelRef();
    static bool &GetEnabledRef();
    static std::vector<LogSource *> &GetSourcesRef();

    static void DefaultCallback(LogLevel level, const char *module, const char *msg);
    void LogImpl(LogLevel level, const char *fmt, va_list args) const;
};

// ============================================================
// 全局日志函数（向后兼容，无模块名）
// ============================================================

inline void LogDebug(const char *fmt, ...)
{
    if (!LogSource::IsEnabled() || LogSource::GetMinLevel() > LogLevel::Debug) return;
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    fprintf(stderr, "[DEBUG] %s\n", buf);
}

inline void Log(const char *fmt, ...)
{
    if (!LogSource::IsEnabled() || LogSource::GetMinLevel() > LogLevel::Info) return;
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    fprintf(stderr, "[INFO ] %s\n", buf);
}

inline void LogWarn(const char *fmt, ...)
{
    if (!LogSource::IsEnabled() || LogSource::GetMinLevel() > LogLevel::Warn) return;
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    fprintf(stderr, "[WARN ] %s\n", buf);
}

inline void LogError(const char *fmt, ...)
{
    if (!LogSource::IsEnabled() || LogSource::GetMinLevel() > LogLevel::Error) return;
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    fprintf(stderr, "[ERROR] %s\n", buf);
}

// ============================================================
// LogSource 实现（inline，因为是 header-only）
// ============================================================

inline LogCallback &LogSource::GetCallback()
{
    static LogCallback cb = DefaultCallback;
    return cb;
}

inline LogLevel &LogSource::GetMinLevelRef()
{
    static LogLevel level = LogLevel::Info;
    return level;
}

inline bool &LogSource::GetEnabledRef()
{
    static bool enabled = true;
    return enabled;
}

inline std::vector<LogSource *> &LogSource::GetSourcesRef()
{
    static std::vector<LogSource *> sources;
    return sources;
}

inline void LogSource::DefaultCallback(LogLevel level, const char *module, const char *msg)
{
    const char *levelStr = "";
    switch (level)
    {
    case LogLevel::Debug: levelStr = "DEBUG"; break;
    case LogLevel::Info:  levelStr = "INFO "; break;
    case LogLevel::Warn:  levelStr = "WARN "; break;
    case LogLevel::Error: levelStr = "ERROR"; break;
    }

    if (module && module[0] != '\0')
        fprintf(stderr, "[%s][%s] %s\n", levelStr, module, msg);
    else
        fprintf(stderr, "[%s] %s\n", levelStr, msg);
}

inline LogSource::LogSource(const char *moduleName) : m_ModuleName(moduleName)
{
    GetSourcesRef().push_back(this);
}

inline LogSource::~LogSource()
{
    auto &sources = GetSourcesRef();
    sources.erase(std::remove(sources.begin(), sources.end(), this), sources.end());
}

inline void LogSource::Debug(const char *fmt, ...) const
{
    va_list args;
    va_start(args, fmt);
    LogImpl(LogLevel::Debug, fmt, args);
    va_end(args);
}

inline void LogSource::Info(const char *fmt, ...) const
{
    va_list args;
    va_start(args, fmt);
    LogImpl(LogLevel::Info, fmt, args);
    va_end(args);
}

inline void LogSource::Warn(const char *fmt, ...) const
{
    va_list args;
    va_start(args, fmt);
    LogImpl(LogLevel::Warn, fmt, args);
    va_end(args);
}

inline void LogSource::Error(const char *fmt, ...) const
{
    va_list args;
    va_start(args, fmt);
    LogImpl(LogLevel::Error, fmt, args);
    va_end(args);
}

inline void LogSource::SetCallback(LogCallback callback) { GetCallback() = callback; }
inline void LogSource::SetMinLevel(LogLevel level) { GetMinLevelRef() = level; }
inline LogLevel LogSource::GetMinLevel() { return GetMinLevelRef(); }
inline void LogSource::SetEnabled(bool enabled) { GetEnabledRef() = enabled; }
inline bool LogSource::IsEnabled() { return GetEnabledRef(); }
inline const std::vector<LogSource *> &LogSource::GetAll() { return GetSourcesRef(); }

inline void LogSource::LogImpl(LogLevel level, const char *fmt, va_list args) const
{
    if (!GetEnabledRef() || level < GetMinLevelRef()) return;
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    GetCallback()(level, m_ModuleName, buf);
}
