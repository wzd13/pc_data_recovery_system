#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace pcdr::fileutil {

bool DirectoryExists(const std::wstring& path);
bool FileExists(const std::wstring& path);
bool EnsureDirectory(const std::wstring& path);
bool EnsureParentDirectory(const std::wstring& filePath);
std::wstring Combine(const std::wstring& a, const std::wstring& b);
std::optional<std::vector<std::uint8_t>> ReadFileBytes(const std::wstring& path, std::size_t maxBytes = 0);
bool WriteFileBytes(const std::wstring& path, const std::uint8_t* data, std::size_t size);
bool WriteTextFile(const std::wstring& path, const std::wstring& text);
std::wstring ReadTextFile(const std::wstring& path);
std::wstring GetExtension(const std::wstring& path);
std::wstring GetFileName(const std::wstring& path);
std::wstring MakeUniquePath(const std::wstring& folder, const std::wstring& fileName);

} // namespace pcdr::fileutil
