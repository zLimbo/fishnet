#pragma once

#include "fishnet/base/log_stream.h"
#include "fishnet/base/timestamp.h"

namespace fishnet {
class TimeZone;

class Logger {
public:
    enum class LogLevel {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        NUM_LOG_LEVELS,
    };

    // 编译时计算源文件 basename
    class SourceFile {
    public:
        template <int N>
        SourceFile(const char (&arr)[N]) : data_(arr), size_(N - 1) {
            const char* slash = strrchr(data_, '/');
            if (slash) {
                data_ = slash + 1;
                size_ -= static_cast<int>(data_ - arr);
            }
        }

        explicit SourceFile(const char* filename) : data_(filename) {
            const char* slash = strrchr(filename, '/');
            if (slash) {
                data_ = slash + 1;
            }
            size_ = static_cast<int>(strlen(data_));
        }

        const char* data_;
        int size_;
    };

    Logger(SourceFile file, int line);
    Logger(SourceFile file, int line, LogLevel level);
    Logger(SourceFile file, int line, LogLevel level, const char* func);
    Logger(SourceFile file, int line, bool toAbort);
    ~Logger();

    LogStream& stream() {
        return impl_.stream_;
    }

    static LogLevel logLevel();
    static void setLogLevel(LogLevel level);

    typedef void (*OutputFunc)(const char* msg, int len);
    typedef void (*FlushFunc)();
    static void setOutput(OutputFunc);
    static void setFlush(FlushFunc);
    static void setTimeZone(const TimeZone& tz);

private:
    class Impl {
    public:
        using LogLevel = Logger::LogLevel;
        Impl(LogLevel level, int old_errno, const SourceFile& file, int line);
        void formatTime();
        void finish();

        Timestamp time_;
        LogStream stream_;
        LogLevel level_;
        int line_;
        SourceFile basename_;
    };

    Impl impl_;
};

extern Logger::LogLevel g_logLevel;

inline Logger::LogLevel Logger::logLevel() {
    return g_logLevel;
}

//
// CAUTION: do not write:
//
// if (good)
//   LOG_INFO << "Good news";
// else
//   LOG_WARN << "Bad news";
//
// this expends to
//
// if (good)
//   if (logging_INFO)
//     logInfoStream << "Good news";
//   else
//     logWarnStream << "Bad news";
//
#define LOG_TRACE                                                         \
    if (fishnet::Logger::logLevel() <= fishnet::Logger::LogLevel::TRACE)  \
    fishnet::Logger(__FILE__, __LINE__, fishnet::Logger::LogLevel::TRACE, \
                    __func__)                                             \
        .stream()
#define LOG_DEBUG                                                         \
    if (fishnet::Logger::logLevel() <= fishnet::Logger::LogLevel::DEBUG)  \
    fishnet::Logger(__FILE__, __LINE__, fishnet::Logger::LogLevel::DEBUG, \
                    __func__)                                             \
        .stream()
#define LOG_INFO                                                        \
    if (fishnet::Logger::logLevel() <= fishnet::Logger::LogLevel::INFO) \
    fishnet::Logger(__FILE__, __LINE__).stream()
#define LOG_WARN                                                         \
    fishnet::Logger(__FILE__, __LINE__, fishnet::Logger::LogLevel::WARN) \
        .stream()
#define LOG_ERROR                                                         \
    fishnet::Logger(__FILE__, __LINE__, fishnet::Logger::LogLevel::ERROR) \
        .stream()
#define LOG_FATAL                                                         \
    fishnet::Logger(__FILE__, __LINE__, fishnet::Logger::LogLevel::FATAL) \
        .stream()
#define LOG_SYSERR fishnet::Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL fishnet::Logger(__FILE__, __LINE__, true).stream()

const char* strerror_tl(int savedErrno);

// Taken from glog/logging.h
//
// Check that the input is non NULL.  This very useful in constructor
// initializer lists.
#define CHECK_NOTNULL(val)                                                     \
    ::fishnet::CheckNotNull(__FILE__, __LINE__, "'" #val "' Must be non NULL", \
                            (val))

// A small helper for CHECK_NOTNULL().
template <typename T>
T* CheckNotNull(Logger::SourceFile file, int line, const char* names, T* ptr) {
    if (ptr == NULL) {
        Logger(file, line, Logger::LogLevel::FATAL).stream() << names;
    }
    return ptr;
}
}  // namespace fishnet