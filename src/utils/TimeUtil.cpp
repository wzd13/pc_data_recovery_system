#include "utils/TimeUtil.hpp"

#include <windows.h>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace pcdr::timeutil {

std::int64_t NowUnixSeconds() {
    return static_cast<std::int64_t>(std::time(nullptr));
}

std::wstring FormatUnixLocal(std::int64_t unixSeconds) {
    std::time_t t = static_cast<std::time_t>(unixSeconds);
    std::tm local{};
    localtime_s(&local, &t);
    std::wstringstream ss;
    ss << std::put_time(&local, L"%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::wstring FormatNowLocal() {
    return FormatUnixLocal(NowUnixSeconds());
}

std::optional<std::int64_t> FileTimeToUnix(std::uint64_t fileTime) {
    if (fileTime == 0) return std::nullopt;
    // FILETIME is 100-ns intervals since 1601-01-01
    constexpr std::uint64_t kEpochDiff = 116444736000000000ULL;
    if (fileTime < kEpochDiff) return std::nullopt;
    return static_cast<std::int64_t>((fileTime - kEpochDiff) / 10000000ULL);
}

std::uint64_t UnixToFileTime(std::int64_t unixSeconds) {
    constexpr std::uint64_t kEpochDiff = 116444736000000000ULL;
    return static_cast<std::uint64_t>(unixSeconds) * 10000000ULL + kEpochDiff;
}

} // namespace pcdr::timeutil
