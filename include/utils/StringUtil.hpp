#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

namespace pcdr::str {

std::wstring Utf8ToWide(const std::string& utf8);
std::string WideToUtf8(const std::wstring& wide);
std::wstring ToLower(const std::wstring& s);
std::wstring Trim(const std::wstring& s);
bool EqualsIgnoreCase(const std::wstring& a, const std::wstring& b);
bool StartsWithIgnoreCase(const std::wstring& s, const std::wstring& prefix);
bool EndsWithIgnoreCase(const std::wstring& s, const std::wstring& suffix);
std::wstring FormatBytes(std::uint64_t bytes);
std::wstring FormatSizeFilter(std::uint64_t bytes);
std::wstring FormatDuration(std::chrono::milliseconds ms);
std::wstring FormatPercent(double pct);
std::wstring FormatSpeed(double bytesPerSecond);
std::wstring Join(const std::vector<std::wstring>& parts, const std::wstring& sep);
std::vector<std::wstring> Split(const std::wstring& s, wchar_t delim);
std::wstring ReplaceAll(std::wstring s, const std::wstring& from, const std::wstring& to);

} // namespace pcdr::str
