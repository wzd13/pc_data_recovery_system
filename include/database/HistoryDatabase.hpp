#pragma once

#include "utils/Types.hpp"
#include <vector>
#include <string>
#include <mutex>

namespace pcdr {

// Lightweight local history store (portable, no external DB dependency).
// Format: UTF-8 lines in data/history.db (pipe-delimited records).
class HistoryDatabase {
public:
    HistoryDatabase();
    bool Open(const std::wstring& path = {});
    bool Add(const HistoryEntry& entry);
    std::vector<HistoryEntry> List(std::size_t limit = 200) const;
    bool Clear();

private:
    std::wstring path_;
    mutable std::mutex mutex_;
    std::wstring defaultPath() const;
};

} // namespace pcdr
