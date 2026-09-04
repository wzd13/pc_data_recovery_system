#include "ui/Theme.hpp"
#include <unordered_map>

namespace pcdr::theme {
namespace {
Colors g_dark;
std::unordered_map<COLORREF, HBRUSH> g_brushes;
HFONT g_title = nullptr;
HFONT g_ui = nullptr;
HFONT g_mono = nullptr;
}

const Colors& Dark() { return g_dark; }

HBRUSH Brush(COLORREF c) {
    auto it = g_brushes.find(c);
    if (it != g_brushes.end()) return it->second;
    HBRUSH b = CreateSolidBrush(c);
    g_brushes[c] = b;
    return b;
}

HFONT TitleFont() {
    if (!g_title) {
        g_title = CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Semibold");
    }
    return g_title;
}

HFONT UiFont() {
    if (!g_ui) {
        g_ui = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    }
    return g_ui;
}

HFONT MonoFont() {
    if (!g_mono) {
        g_mono = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    }
    return g_mono;
}

void Shutdown() {
    for (auto& kv : g_brushes) DeleteObject(kv.second);
    g_brushes.clear();
    if (g_title) { DeleteObject(g_title); g_title = nullptr; }
    if (g_ui) { DeleteObject(g_ui); g_ui = nullptr; }
    if (g_mono) { DeleteObject(g_mono); g_mono = nullptr; }
}

} // namespace pcdr::theme
