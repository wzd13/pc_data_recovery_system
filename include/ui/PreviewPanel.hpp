#pragma once

#include "preview/PreviewEngine.hpp"
#include <windows.h>

namespace pcdr {

class PreviewPanel {
public:
    void Create(HWND parent, int idHost);
    void Layout(RECT rc);
    void Show(const PreviewData& data);
    void Clear();
    HWND Host() const { return host_; }

private:
    HWND parent_ = nullptr;
    HWND host_ = nullptr;
    HWND title_ = nullptr;
    HWND info_ = nullptr;
    PreviewData current_{};
    static LRESULT CALLBACK HostProc(HWND, UINT, WPARAM, LPARAM);
    WNDPROC oldProc_ = nullptr;
};

} // namespace pcdr
