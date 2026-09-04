#pragma once

#include <string>
#include <cstdint>
#include <optional>

namespace pcdr::timeutil {

std::int64_t NowUnixSeconds();
std::wstring FormatUnixLocal(std::int64_t unixSeconds);
std::wstring FormatNowLocal();
std::optional<std::int64_t> FileTimeToUnix(std::uint64_t fileTime);
std::uint64_t UnixToFileTime(std::int64_t unixSeconds);

} // namespace pcdr::timeutil
