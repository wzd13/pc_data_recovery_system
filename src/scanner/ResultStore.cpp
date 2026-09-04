#include "scanner/ResultStore.hpp"
#include "utils/StringUtil.hpp"
#include "utils/FileUtil.hpp"

namespace pcdr {

void ResultStore::Clear() {
    std::lock_guard lock(mutex_);
    files_.clear();
    nextId_ = 1;
}

std::uint64_t ResultStore::Add(RecoveredFile file) {
    std::lock_guard lock(mutex_);
    file.id = nextId_++;
    files_.push_back(std::move(file));
    return files_.back().id;
}

std::size_t ResultStore::Count() const {
    std::lock_guard lock(mutex_);
    return files_.size();
}

std::vector<RecoveredFile> ResultStore::All() const {
    std::lock_guard lock(mutex_);
    return files_;
}

std::optional<RecoveredFile> ResultStore::Get(std::uint64_t id) const {
    std::lock_guard lock(mutex_);
    for (const auto& f : files_) if (f.id == id) return f;
    return std::nullopt;
}

bool ResultStore::UpdateStatus(std::uint64_t id, RecoveryStatus status) {
    std::lock_guard lock(mutex_);
    for (auto& f : files_) {
        if (f.id == id) {
            f.status = status;
            return true;
        }
    }
    return false;
}

std::vector<RecoveredFile> ResultStore::GetByIds(const std::vector<std::uint64_t>& ids) const {
    std::lock_guard lock(mutex_);
    std::vector<RecoveredFile> out;
    for (auto id : ids) {
        for (const auto& f : files_) {
            if (f.id == id) {
                out.push_back(f);
                break;
            }
        }
    }
    return out;
}

std::vector<RecoveredFile> ResultStore::Query(const ResultFilter& filter, SortKey sort, bool ascending) const {
    std::lock_guard lock(mutex_);
    std::vector<RecoveredFile> out;
    out.reserve(files_.size());

    for (const auto& f : files_) {
        if (!filter.searchName.empty()) {
            if (str::ToLower(f.displayName).find(str::ToLower(filter.searchName)) == std::wstring::npos &&
                str::ToLower(f.originalPath).find(str::ToLower(filter.searchName)) == std::wstring::npos) {
                continue;
            }
        }
        if (!filter.extension.empty() && !str::EqualsIgnoreCase(f.extension, filter.extension)) continue;
        if (filter.minSize && f.sizeBytes < *filter.minSize) continue;
        if (filter.maxSize && f.sizeBytes > *filter.maxSize) continue;
        if (filter.minDate && (!f.modifiedUtc || *f.modifiedUtc < *filter.minDate)) continue;
        if (filter.maxDate && (!f.modifiedUtc || *f.modifiedUtc > *filter.maxDate)) continue;
        if (filter.confidence && f.confidence != *filter.confidence) continue;
        if (filter.method && f.method != *filter.method) continue;
        out.push_back(f);
    }

    auto cmp = [&](const RecoveredFile& a, const RecoveredFile& b) {
        int r = 0;
        switch (sort) {
        case SortKey::Name:
            r = _wcsicmp(a.displayName.c_str(), b.displayName.c_str());
            break;
        case SortKey::Size:
            r = (a.sizeBytes < b.sizeBytes) ? -1 : (a.sizeBytes > b.sizeBytes ? 1 : 0);
            break;
        case SortKey::Date: {
            const auto da = a.modifiedUtc.value_or(0);
            const auto db = b.modifiedUtc.value_or(0);
            r = (da < db) ? -1 : (da > db ? 1 : 0);
            break;
        }
        case SortKey::Type:
            r = _wcsicmp(a.extension.c_str(), b.extension.c_str());
            break;
        case SortKey::Confidence:
            r = static_cast<int>(a.confidence) - static_cast<int>(b.confidence);
            break;
        }
        return ascending ? (r < 0) : (r > 0);
    };
    std::sort(out.begin(), out.end(), cmp);
    return out;
}

} // namespace pcdr
