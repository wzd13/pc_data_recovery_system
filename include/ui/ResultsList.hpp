#pragma once

#include "utils/Types.hpp"
#include "scanner/ResultStore.hpp"
#include <windows.h>
#include <vector>

namespace pcdr {

class ResultsList {
public:
    void Create(HWND parent, int idList, int idSearch, int idType, int idConf, int idSort);
    void Layout(RECT rc);
    void SetFiles(const std::vector<RecoveredFile>& files);
    void ApplyFilter();
    std::vector<std::uint64_t> SelectedIds() const;
    void SelectAll();
    std::optional<std::uint64_t> FocusedId() const;
    HWND ListHwnd() const { return list_; }
    ResultFilter CurrentFilter() const;
    SortKey CurrentSort() const;
    bool SortAscending() const { return ascending_; }

private:
    void RebuildView();
    HWND parent_ = nullptr;
    HWND list_ = nullptr;
    HWND search_ = nullptr;
    HWND type_ = nullptr;
    HWND conf_ = nullptr;
    HWND sort_ = nullptr;
    HWND label_ = nullptr;
    std::vector<RecoveredFile> view_;
    bool ascending_ = true;
};

} // namespace pcdr
