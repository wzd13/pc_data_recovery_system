#pragma once

#include <string>

namespace pcdr::pathutil {

std::wstring GetExecutableDirectory();
std::wstring GetAppRootDirectory();
std::wstring GetConfigDirectory();
std::wstring GetDataDirectory();
std::wstring GetLogsDirectory();
std::wstring NormalizeDriveRoot(const std::wstring& letterOrPath);

} // namespace pcdr::pathutil
