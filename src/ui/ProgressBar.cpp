#include "ui/ProgressBar.hpp"
#include "ui/Controls.hpp"
#include "ui/Theme.hpp"
#include "utils/StringUtil.hpp"

namespace pcdr {

void ProgressBarPanel::Create(HWND parent, int idStatus) {
    parent_ = parent;
    barHost_ = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, 0, 100, 18, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    SetWindowLongPtrW(barHost_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    oldProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(barHost_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(BarProc)));

    status_ = ui::CreateDarkStatic(parent, idStatus, L"Ready — select a drive and scan mode.", 0, 0, 100, 20);
}

void ProgressBarPanel::Layout(RECT rc) {
    MoveWindow(barHost_, rc.left, rc.top + 4, rc.right - rc.left, 16, TRUE);
    MoveWindow(status_, rc.left, rc.top + 24, rc.right - rc.left, 20, TRUE);
}

void ProgressBarPanel::Update(const ScanStats& stats) {
    percent_ = stats.progressPercent;
    std::wstring text = stats.statusText;
    if (stats.state == ScanState::Running || stats.state == ScanState::Paused) {
        text += L"  |  " + str::FormatPercent(stats.progressPercent);
        text += L"  |  Found: " + std::to_wstring(stats.filesFound);
        text += L"  |  " + str::FormatSpeed(stats.bytesPerSecond);
        text += L"  |  Elapsed: " + str::FormatDuration(stats.elapsed);
        text += L"  |  ETA: " + str::FormatDuration(stats.eta);
    } else if (stats.state == ScanState::Completed) {
        text += L"  |  Files found: " + std::to_wstring(stats.filesFound);
        text += L"  |  " + str::FormatDuration(stats.elapsed);
    }
    SetWindowTextW(status_, text.c_str());
    InvalidateRect(barHost_, nullptr, TRUE);
}

void ProgressBarPanel::SetStatus(const std::wstring& text) {
    SetWindowTextW(status_, text.c_str());
}

LRESULT CALLBACK ProgressBarPanel::BarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<ProgressBarPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, theme::Brush(theme::Dark().panelAlt));
        RECT fill = rc;
        fill.right = rc.left + static_cast<LONG>((rc.right - rc.left) * (self->percent_ / 100.0));
        FillRect(hdc, &fill, theme::Brush(theme::Dark().accent));
        FrameRect(hdc, &rc, theme::Brush(theme::Dark().border));
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return CallWindowProcW(self->oldProc_, hwnd, msg, wParam, lParam);
}

} // namespace pcdr
