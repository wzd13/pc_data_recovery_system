#include "utils/StringUtil.hpp"

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace pcdr::str {

std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), needed);
    return out;
}

std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string out(needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), needed, nullptr, nullptr);
    return out;
}

std::wstring ToLower(const std::wstring& s) {
    std::wstring out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(towlower(c));
    });
    return out;
}

std::wstring Trim(const std::wstring& s) {
    const auto start = s.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return {};
    const auto end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool EqualsIgnoreCase(const std::wstring& a, const std::wstring& b) {
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

bool StartsWithIgnoreCase(const std::wstring& s, const std::wstring& prefix) {
    if (prefix.size() > s.size()) return false;
    return _wcsnicmp(s.c_str(), prefix.c_str(), prefix.size()) == 0;
}

bool EndsWithIgnoreCase(const std::wstring& s, const std::wstring& suffix) {
    if (suffix.size() > s.size()) return false;
    return _wcsicmp(s.c_str() + (s.size() - suffix.size()), suffix.c_str()) == 0;
}

std::wstring FormatBytes(std::uint64_t bytes) {
    static const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB", L"PB" };
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 5) {
        value /= 1024.0;
        ++unit;
    }
    std::wstringstream ss;
    if (unit == 0) ss << bytes << L" B";
    else ss << std::fixed << std::setprecision(value >= 100 ? 0 : (value >= 10 ? 1 : 2)) << value << L" " << units[unit];
    return ss.str();
}

std::wstring FormatSizeFilter(std::uint64_t bytes) {
    return FormatBytes(bytes);
}

std::wstring FormatDuration(std::chrono::milliseconds ms) {
    const auto totalSec = ms.count() / 1000;
    const auto h = totalSec / 3600;
    const auto m = (totalSec % 3600) / 60;
    const auto s = totalSec % 60;
    std::wstringstream ss;
    if (h > 0) ss << h << L"h " << m << L"m " << s << L"s";
    else if (m > 0) ss << m << L"m " << s << L"s";
    else ss << s << L"s";
    return ss.str();
}

std::wstring FormatPercent(double pct) {
    std::wstringstream ss;
    ss << std::fixed << std::setprecision(1) << pct << L"%";
    return ss.str();
}

std::wstring FormatSpeed(double bytesPerSecond) {
    return FormatBytes(static_cast<std::uint64_t>(std::max(0.0, bytesPerSecond))) + L"/s";
}

std::wstring Join(const std::vector<std::wstring>& parts, const std::wstring& sep) {
    std::wstring out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

std::vector<std::wstring> Split(const std::wstring& s, wchar_t delim) {
    std::vector<std::wstring> out;
    std::wstring cur;
    for (wchar_t c : s) {
        if (c == delim) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

std::wstring ReplaceAll(std::wstring s, const std::wstring& from, const std::wstring& to) {
    if (from.empty()) return s;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::wstring::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

} // namespace pcdr::str
