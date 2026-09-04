#include "ui/Controls.hpp"
#include "ui/Theme.hpp"

namespace pcdr::ui {

void SetControlFont(HWND hwnd) {
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(theme::UiFont()), TRUE);
}

HWND CreateDarkButton(HWND parent, int id, const std::wstring& text, int x, int y, int w, int h) {
    HWND hwnd = CreateWindowExW(0, L"BUTTON", text.c_str(),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SetControlFont(hwnd);
    return hwnd;
}

HWND CreateDarkStatic(HWND parent, int id, const std::wstring& text, int x, int y, int w, int h) {
    HWND hwnd = CreateWindowExW(0, L"STATIC", text.c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SetControlFont(hwnd);
    return hwnd;
}

HWND CreateDarkEdit(HWND parent, int id, int x, int y, int w, int h) {
    HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SetControlFont(hwnd);
    return hwnd;
}

HWND CreateDarkCombo(HWND parent, int id, int x, int y, int w, int h) {
    HWND hwnd = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SetControlFont(hwnd);
    return hwnd;
}

LRESULT HandleCtlColor(WPARAM wParam, LPARAM lParam) {
    HDC hdc = reinterpret_cast<HDC>(wParam);
    SetTextColor(hdc, theme::Dark().text);
    SetBkColor(hdc, theme::Dark().panel);
    return reinterpret_cast<LRESULT>(theme::Brush(theme::Dark().panel));
}

} // namespace pcdr::ui
