#include "utils/PathUtil.hpp"
#include "utils/FileUtil.hpp"

#include <windows.h>
#include <shlwapi.h>

namespace pcdr::pathutil {

std::wstring GetExecutableDirectory() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::wstring path(buffer, len);
    const auto pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L".";
    return path.substr(0, pos);
}

std::wstring GetAppRootDirectory() {
    // Prefer directory containing the EXE (portable layout).
    return GetExecutableDirectory();
}

std::wstring GetConfigDirectory() {
    const auto dir = fileutil::Combine(GetAppRootDirectory(), L"config");
    fileutil::EnsureDirectory(dir);
    return dir;
}

std::wstring GetDataDirectory() {
    const auto dir = fileutil::Combine(GetAppRootDirectory(), L"data");
    fileutil::EnsureDirectory(dir);
    return dir;
}

std::wstring GetLogsDirectory() {
    const auto dir = fileutil::Combine(GetAppRootDirectory(), L"logs");
    fileutil::EnsureDirectory(dir);
    return dir;
}

std::wstring NormalizeDriveRoot(const std::wstring& letterOrPath) {
    if (letterOrPath.empty()) return {};
    if (letterOrPath.size() >= 2 && letterOrPath[1] == L':') {
        return std::wstring(1, towupper(letterOrPath[0])) + L":";
    }
    return letterOrPath;
}

} // namespace pcdr::pathutil
