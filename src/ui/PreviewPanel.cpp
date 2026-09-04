#include "ui/PreviewPanel.hpp"
#include "ui/Controls.hpp"
#include "ui/Theme.hpp"
#include "utils/StringUtil.hpp"

#include <algorithm>

namespace pcdr {

void PreviewPanel::Create(HWND parent, int idHost) {
    parent_ = parent;
    title_ = ui::CreateDarkStatic(parent, -1, L"Preview", 0, 0, 100, 22);
    SendMessageW(title_, WM_SETFONT, reinterpret_cast<WPARAM>(theme::TitleFont()), TRUE);

    host_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, 0, 100, 100, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(idHost)),
        GetModuleHandleW(nullptr), nullptr);
    SetWindowLongPtrW(host_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    oldProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(host_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HostProc)));

    info_ = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        0, 0, 100, 100, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    ui::SetControlFont(info_);
}

void PreviewPanel::Layout(RECT rc) {
    MoveWindow(title_, rc.left, rc.top, rc.right - rc.left, 24, TRUE);
    const int split = (rc.bottom - rc.top) / 2;
    MoveWindow(host_, rc.left, rc.top + 28, rc.right - rc.left, split - 10, TRUE);
    MoveWindow(info_, rc.left, rc.top + split + 24, rc.right - rc.left, rc.bottom - (rc.top + split + 24), TRUE);
}

void PreviewPanel::Clear() {
    if (current_.imageBitmap) {
        DeleteObject(current_.imageBitmap);
        current_.imageBitmap = nullptr;
    }
    current_ = {};
    SetWindowTextW(info_, L"");
    InvalidateRect(host_, nullptr, TRUE);
}

void PreviewPanel::Show(const PreviewData& data) {
    if (current_.imageBitmap) {
        DeleteObject(current_.imageBitmap);
        current_.imageBitmap = nullptr;
    }
    current_ = data;

    std::wstring text;
    text += L"Name: " + data.fileName + L"\r\n";
    text += L"Path: " + data.path + L"\r\n";
    text += L"Type: " + data.typeLabel + L"\r\n";
    text += L"Size: " + str::FormatBytes(data.sizeBytes) + L"\r\n";
    text += L"Confidence: " + data.confidence + L"\r\n";
    if (data.imageWidth > 0) {
        text += L"Dimensions: " + std::to_wstring(data.imageWidth) + L" x " +
                std::to_wstring(data.imageHeight) + L"\r\n";
    }
    if (!data.infoMessage.empty()) text += L"\r\n" + data.infoMessage + L"\r\n";
    if (data.kind == PreviewData::Kind::Text) text += L"\r\n--- Text Preview ---\r\n" + data.textContent;
    SetWindowTextW(info_, text.c_str());
    InvalidateRect(host_, nullptr, TRUE);
}

LRESULT CALLBACK PreviewPanel::HostProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<PreviewPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, theme::Brush(theme::Dark().panelAlt));

        if (self->current_.kind == PreviewData::Kind::Image && self->current_.imageBitmap) {
            HDC mem = CreateCompatibleDC(hdc);
            HGDIOBJ old = SelectObject(mem, self->current_.imageBitmap);
            BITMAP bm{};
            GetObject(self->current_.imageBitmap, sizeof(bm), &bm);
            const int margin = 8;
            const int availW = rc.right - rc.left - margin * 2;
            const int availH = rc.bottom - rc.top - margin * 2;
            double scale = std::min(availW / static_cast<double>(bm.bmWidth),
                                    availH / static_cast<double>(bm.bmHeight));
            if (scale > 1.0) scale = 1.0;
            const int dw = static_cast<int>(bm.bmWidth * scale);
            const int dh = static_cast<int>(bm.bmHeight * scale);
            const int dx = margin + (availW - dw) / 2;
            const int dy = margin + (availH - dh) / 2;
            SetStretchBltMode(hdc, HALFTONE);
            StretchBlt(hdc, dx, dy, dw, dh, mem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
            SelectObject(mem, old);
            DeleteDC(mem);
        } else {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, theme::Dark().textDim);
            const wchar_t* msgText = L"No preview";
            if (self->current_.kind == PreviewData::Kind::PdfInfo) msgText = L"PDF metadata preview";
            else if (self->current_.kind == PreviewData::Kind::Text) msgText = L"Text preview in panel below";
            else if (self->current_.kind == PreviewData::Kind::Unsupported) msgText = L"Preview unsupported";
            DrawTextW(hdc, msgText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    if (msg == WM_ERASEBKGND) return 1;
    return CallWindowProcW(self->oldProc_, hwnd, msg, wParam, lParam);
}

} // namespace pcdr
