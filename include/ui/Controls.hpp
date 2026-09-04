#pragma once

#include <windows.h>
#include <string>

namespace pcdr::ui {

HWND CreateDarkButton(HWND parent, int id, const std::wstring& text, int x, int y, int w, int h);
HWND CreateDarkStatic(HWND parent, int id, const std::wstring& text, int x, int y, int w, int h);
HWND CreateDarkEdit(HWND parent, int id, int x, int y, int w, int h);
HWND CreateDarkCombo(HWND parent, int id, int x, int y, int w, int h);
void SetControlFont(HWND hwnd);
LRESULT HandleCtlColor(WPARAM wParam, LPARAM lParam);

} // namespace pcdr::ui
