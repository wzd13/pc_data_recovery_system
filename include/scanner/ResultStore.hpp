#pragma once

#include "utils/Types.hpp"
#include <vector>
#include <mutex>
#include <string>
#include <optional>
#include <algorithm>

namespace pcdr {

struct ResultFilter {
    std::wstring searchName;
    std::wstring extension; // empty = all
    std::optional<std::uint64_t> minSize;
    std::optional<std::uint64_t> maxSize;
    std::optional<std::int64_t> minDate;
    std::optional<std::int64_t> maxDate;
    std::optional<Confidence> confidence;
    std::optional<DiscoveryMethod> method;
};

enum class SortKey { Name, Size, Date, Type, Confidence };

class ResultStore {
public:
    void Clear();
    std::uint64_t Add(RecoveredFile file);
    std::size_t Count() const;
    std::vector<RecoveredFile> All() const;
    std::optional<RecoveredFile> Get(std::uint64_t id) const;
    bool UpdateStatus(std::uint64_t id, RecoveryStatus status);
    std::vector<RecoveredFile> Query(const ResultFilter& filter, SortKey sort, bool ascending) const;
    std::vector<RecoveredFile> GetByIds(const std::vector<std::uint64_t>& ids) const;

private:
    mutable std::mutex mutex_;
    std::vector<RecoveredFile> files_;
    std::uint64_t nextId_ = 1;
};

} // namespace pcdr
