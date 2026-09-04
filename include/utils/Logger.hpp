#pragma once

#include <string>
#include <mutex>

namespace pcdr {

enum class LogLevel { Debug, Info, Warning, Error };

class Logger {
public:
    static Logger& Instance();

    void SetLogDirectory(const std::wstring& dir);
    void Log(LogLevel level, const std::wstring& message);
    void Info(const std::wstring& message);
    void Warning(const std::wstring& message);
    void Error(const std::wstring& message);
    void Debug(const std::wstring& message);

private:
    Logger() = default;
    std::wstring levelName(LogLevel level) const;
    std::wstring logDir_;
    std::mutex mutex_;
};

} // namespace pcdr
