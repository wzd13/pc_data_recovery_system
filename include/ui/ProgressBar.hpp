#pragma once

#include "utils/Types.hpp"
#include <windows.h>
#include <string>

namespace pcdr {

class ProgressBarPanel {
public:
    void Create(HWND parent, int idStatus);
    void Layout(RECT rc);
    void Update(const ScanStats& stats);
    void SetStatus(const std::wstring& text);

private:
    HWND parent_ = nullptr;
    HWND status_ = nullptr;
    HWND barHost_ = nullptr;
    double percent_ = 0;
    static LRESULT CALLBACK BarProc(HWND, UINT, WPARAM, LPARAM);
    WNDPROC oldProc_ = nullptr;
};

} // namespace pcdr
