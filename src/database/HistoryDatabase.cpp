#include "database/HistoryDatabase.hpp"
#include "utils/PathUtil.hpp"
#include "utils/FileUtil.hpp"
#include "utils/StringUtil.hpp"
#include "utils/TimeUtil.hpp"

#include <sstream>
#include <algorithm>

namespace pcdr {

HistoryDatabase::HistoryDatabase() = default;

std::wstring HistoryDatabase::defaultPath() const {
    return fileutil::Combine(pathutil::GetDataDirectory(), L"history.db");
}

bool HistoryDatabase::Open(const std::wstring& path) {
    std::lock_guard lock(mutex_);
    path_ = path.empty() ? defaultPath() : path;
    fileutil::EnsureParentDirectory(path_);
    if (!fileutil::FileExists(path_)) {
        return fileutil::WriteTextFile(path_, L"# PCDataRecovery history\n");
    }
    return true;
}

bool HistoryDatabase::Add(const HistoryEntry& entry) {
    std::lock_guard lock(mutex_);
    if (path_.empty() && !Open({})) return false;

    auto existing = fileutil::ReadTextFile(path_);
    std::wstringstream line;
    line << entry.id << L'|'
         << entry.timestampUtc << L'|'
         << str::ReplaceAll(entry.sourceDrive, L"|", L"/") << L'|'
         << str::ReplaceAll(entry.destination, L"|", L"/") << L'|'
         << entry.filesRequested << L'|'
         << entry.successCount << L'|'
         << entry.failureCount << L'|'
         << entry.durationMs << L'|'
         << str::ReplaceAll(entry.notes, L"|", L" ") << L"\n";

    existing += line.str();
    return fileutil::WriteTextFile(path_, existing);
}

std::vector<HistoryEntry> HistoryDatabase::List(std::size_t limit) const {
    std::lock_guard lock(mutex_);
    std::vector<HistoryEntry> out;
    if (path_.empty()) return out;
    const auto text = fileutil::ReadTextFile(path_);
    for (const auto& raw : str::Split(text, L'\n')) {
        auto line = str::Trim(str::ReplaceAll(raw, L"\r", L""));
        if (line.empty() || line[0] == L'#') continue;
        auto parts = str::Split(line, L'|');
        if (parts.size() < 8) continue;
        HistoryEntry e;
        try {
            e.id = std::stoull(str::WideToUtf8(parts[0]));
            e.timestampUtc = std::stoll(str::WideToUtf8(parts[1]));
            e.sourceDrive = parts[2];
            e.destination = parts[3];
            e.filesRequested = static_cast<std::uint32_t>(std::stoul(str::WideToUtf8(parts[4])));
            e.successCount = static_cast<std::uint32_t>(std::stoul(str::WideToUtf8(parts[5])));
            e.failureCount = static_cast<std::uint32_t>(std::stoul(str::WideToUtf8(parts[6])));
            e.durationMs = static_cast<std::uint32_t>(std::stoul(str::WideToUtf8(parts[7])));
            if (parts.size() > 8) e.notes = parts[8];
            out.push_back(e);
        } catch (...) {
            continue;
        }
    }
    std::reverse(out.begin(), out.end());
    if (out.size() > limit) out.resize(limit);
    return out;
}

bool HistoryDatabase::Clear() {
    std::lock_guard lock(mutex_);
    if (path_.empty()) path_ = defaultPath();
    return fileutil::WriteTextFile(path_, L"# PCDataRecovery history\n");
}

} // namespace pcdr
