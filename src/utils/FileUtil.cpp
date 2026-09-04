#include "utils/FileUtil.hpp"
#include "utils/StringUtil.hpp"

#include <windows.h>
#include <fstream>
#include <sstream>

namespace pcdr::fileutil {

bool DirectoryExists(const std::wstring& path) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool FileExists(const std::wstring& path) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool EnsureDirectory(const std::wstring& path) {
    if (path.empty()) return false;
    if (DirectoryExists(path)) return true;

    std::wstring normalized = path;
    for (auto& c : normalized) if (c == L'/') c = L'\\';

    std::wstring current;
    for (std::size_t i = 0; i < normalized.size(); ++i) {
        current.push_back(normalized[i]);
        if (normalized[i] == L'\\' || i + 1 == normalized.size()) {
            if (current.size() >= 3) { // skip "C:"
                if (!DirectoryExists(current)) {
                    if (!CreateDirectoryW(current.c_str(), nullptr)) {
                        const DWORD err = GetLastError();
                        if (err != ERROR_ALREADY_EXISTS) return false;
                    }
                }
            }
        }
    }
    return DirectoryExists(path);
}

bool EnsureParentDirectory(const std::wstring& filePath) {
    const auto pos = filePath.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return true;
    return EnsureDirectory(filePath.substr(0, pos));
}

std::wstring Combine(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    const bool aSlash = a.back() == L'\\' || a.back() == L'/';
    const bool bSlash = b.front() == L'\\' || b.front() == L'/';
    if (aSlash && bSlash) return a + b.substr(1);
    if (!aSlash && !bSlash) return a + L"\\" + b;
    return a + b;
}

std::optional<std::vector<std::uint8_t>> ReadFileBytes(const std::wstring& path, std::size_t maxBytes) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return std::nullopt;

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size)) {
        CloseHandle(h);
        return std::nullopt;
    }

    std::size_t toRead = static_cast<std::size_t>(size.QuadPart);
    if (maxBytes > 0 && toRead > maxBytes) toRead = maxBytes;

    std::vector<std::uint8_t> buffer(toRead);
    std::size_t offset = 0;
    while (offset < toRead) {
        DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(toRead - offset, 1024 * 1024));
        DWORD read = 0;
        if (!ReadFile(h, buffer.data() + offset, chunk, &read, nullptr) || read == 0) break;
        offset += read;
    }
    CloseHandle(h);
    buffer.resize(offset);
    return buffer;
}

bool WriteFileBytes(const std::wstring& path, const std::uint8_t* data, std::size_t size) {
    if (!EnsureParentDirectory(path)) return false;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    std::size_t offset = 0;
    while (offset < size) {
        DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(size - offset, 1024 * 1024));
        DWORD written = 0;
        if (!WriteFile(h, data + offset, chunk, &written, nullptr)) {
            CloseHandle(h);
            return false;
        }
        offset += written;
    }
    CloseHandle(h);
    return true;
}

bool WriteTextFile(const std::wstring& path, const std::wstring& text) {
    const auto utf8 = str::WideToUtf8(text);
    return WriteFileBytes(path, reinterpret_cast<const std::uint8_t*>(utf8.data()), utf8.size());
}

std::wstring ReadTextFile(const std::wstring& path) {
    auto bytes = ReadFileBytes(path);
    if (!bytes) return {};
    return str::Utf8ToWide(std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

std::wstring GetExtension(const std::wstring& path) {
    const auto name = GetFileName(path);
    const auto pos = name.find_last_of(L'.');
    if (pos == std::wstring::npos || pos + 1 >= name.size()) return {};
    return str::ToLower(name.substr(pos + 1));
}

std::wstring GetFileName(const std::wstring& path) {
    const auto pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return path;
    return path.substr(pos + 1);
}

std::wstring MakeUniquePath(const std::wstring& folder, const std::wstring& fileName) {
    auto candidate = Combine(folder, fileName);
    if (!FileExists(candidate)) return candidate;

    const auto dot = fileName.find_last_of(L'.');
    const std::wstring stem = (dot == std::wstring::npos) ? fileName : fileName.substr(0, dot);
    const std::wstring ext = (dot == std::wstring::npos) ? L"" : fileName.substr(dot);

    for (int i = 1; i < 100000; ++i) {
        candidate = Combine(folder, stem + L" (" + std::to_wstring(i) + L")" + ext);
        if (!FileExists(candidate)) return candidate;
    }
    return Combine(folder, stem + L"_" + std::to_wstring(GetTickCount64()) + ext);
}

} // namespace pcdr::fileutil
