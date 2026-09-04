#include "utils/Logger.hpp"
#include "utils/PathUtil.hpp"
#include "utils/FileUtil.hpp"
#include "utils/TimeUtil.hpp"
#include "utils/StringUtil.hpp"

#include <fstream>
#include <windows.h>

namespace pcdr {

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

void Logger::SetLogDirectory(const std::wstring& dir) {
    std::lock_guard lock(mutex_);
    logDir_ = dir;
    fileutil::EnsureDirectory(dir);
}

std::wstring Logger::levelName(LogLevel level) const {
    switch (level) {
    case LogLevel::Debug: return L"DEBUG";
    case LogLevel::Info: return L"INFO";
    case LogLevel::Warning: return L"WARN";
    case LogLevel::Error: return L"ERROR";
    }
    return L"LOG";
}

void Logger::Log(LogLevel level, const std::wstring& message) {
    std::lock_guard lock(mutex_);
    if (logDir_.empty()) {
        logDir_ = pathutil::GetLogsDirectory();
        fileutil::EnsureDirectory(logDir_);
    }

    const auto path = fileutil::Combine(logDir_, L"pcdr.log");
    const auto line = timeutil::FormatNowLocal() + L" [" + levelName(level) + L"] " + message + L"\r\n";
    const auto utf8 = str::WideToUtf8(line);

    HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
        CloseHandle(h);
    }

    OutputDebugStringW(line.c_str());
}

void Logger::Info(const std::wstring& message) { Log(LogLevel::Info, message); }
void Logger::Warning(const std::wstring& message) { Log(LogLevel::Warning, message); }
void Logger::Error(const std::wstring& message) { Log(LogLevel::Error, message); }
void Logger::Debug(const std::wstring& message) { Log(LogLevel::Debug, message); }

} // namespace pcdr
