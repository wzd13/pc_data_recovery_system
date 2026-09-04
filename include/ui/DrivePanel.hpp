#pragma once

#include "utils/Types.hpp"
#include <windows.h>
#include <vector>
#include <optional>

namespace pcdr {

class DrivePanel {
public:
    void Create(HWND parent, int idList, int idRefresh);
    void Layout(RECT rc);
    void Refresh();
    std::optional<DriveInfo> Selected() const;
    const std::vector<DriveInfo>& Drives() const { return drives_; }
    HWND ListHwnd() const { return list_; }

private:
    HWND parent_ = nullptr;
    HWND list_ = nullptr;
    HWND refresh_ = nullptr;
    HWND title_ = nullptr;
    std::vector<DriveInfo> drives_;
};

} // namespace pcdr
