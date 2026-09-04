#pragma once

#include "database/HistoryDatabase.hpp"
#include <windows.h>

namespace pcdr {

class HistoryDialog {
public:
    static void Show(HWND parent, HistoryDatabase& db);
};

} // namespace pcdr
