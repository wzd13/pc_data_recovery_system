#pragma once

#include <windows.h>
#include <string>

namespace pcdr::theme {

struct Colors {
    COLORREF background = RGB(22, 24, 28);
    COLORREF panel = RGB(32, 35, 42);
    COLORREF panelAlt = RGB(40, 44, 52);
    COLORREF border = RGB(58, 64, 76);
    COLORREF text = RGB(230, 233, 239);
    COLORREF textDim = RGB(160, 168, 180);
    COLORREF accent = RGB(0, 153, 140);      // teal — avoid purple AI cliché
    COLORREF accentHot = RGB(0, 180, 165);
    COLORREF danger = RGB(200, 80, 70);
    COLORREF success = RGB(70, 170, 110);
    COLORREF warning = RGB(210, 160, 60);
    COLORREF listSel = RGB(0, 110, 100);
    COLORREF topBar = RGB(18, 20, 24);
};

const Colors& Dark();
HBRUSH Brush(COLORREF c);
HFONT TitleFont();
HFONT UiFont();
HFONT MonoFont();
void Shutdown();

} // namespace pcdr::theme
