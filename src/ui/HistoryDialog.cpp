#include "ui/HistoryDialog.hpp"
#include "ui/Theme.hpp"
#include "utils/StringUtil.hpp"
#include "utils/TimeUtil.hpp"

#include <commctrl.h>

namespace pcdr {
namespace {

constexpr wchar_t kHistoryClass[] = L"PCDataRecoveryHistoryWnd";
constexpr int kCloseId = 1001;

struct HistoryState {
    bool closing = false;
    HWND list = nullptr;
    HistoryDatabase* db = nullptr;
};

LRESULT CALLBACK HistoryWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<HistoryState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = reinterpret_cast<HistoryState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        state->list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_TABSTOP,
            10, 10, 780, 320, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        ListView_SetExtendedListViewStyle(state->list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        ListView_SetBkColor(state->list, theme::Dark().panel);
        ListView_SetTextBkColor(state->list, theme::Dark().panel);
        ListView_SetTextColor(state->list, theme::Dark().text);

        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = const_cast<LPWSTR>(L"Date/Time"); col.cx = 140; ListView_InsertColumn(state->list, 0, &col);
        col.pszText = const_cast<LPWSTR>(L"Source"); col.cx = 70; ListView_InsertColumn(state->list, 1, &col);
        col.pszText = const_cast<LPWSTR>(L"Destination"); col.cx = 240; ListView_InsertColumn(state->list, 2, &col);
        col.pszText = const_cast<LPWSTR>(L"Files"); col.cx = 60; ListView_InsertColumn(state->list, 3, &col);
        col.pszText = const_cast<LPWSTR>(L"Success"); col.cx = 70; ListView_InsertColumn(state->list, 4, &col);
        col.pszText = const_cast<LPWSTR>(L"Failed"); col.cx = 70; ListView_InsertColumn(state->list, 5, &col);
        col.pszText = const_cast<LPWSTR>(L"Duration"); col.cx = 90; ListView_InsertColumn(state->list, 6, &col);

        if (state->db) {
            auto entries = state->db->List();
            for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                const auto& e = entries[i];
                LVITEMW item{};
                item.mask = LVIF_TEXT;
                item.iItem = i;
                std::wstring date = timeutil::FormatUnixLocal(e.timestampUtc);
                item.pszText = date.data();
                ListView_InsertItem(state->list, &item);
                ListView_SetItemText(state->list, i, 1, const_cast<LPWSTR>(e.sourceDrive.c_str()));
                ListView_SetItemText(state->list, i, 2, const_cast<LPWSTR>(e.destination.c_str()));
                std::wstring req = std::to_wstring(e.filesRequested);
                ListView_SetItemText(state->list, i, 3, req.data());
                std::wstring ok = std::to_wstring(e.successCount);
                ListView_SetItemText(state->list, i, 4, ok.data());
                std::wstring fail = std::to_wstring(e.failureCount);
                ListView_SetItemText(state->list, i, 5, fail.data());
                std::wstring dur = str::FormatDuration(std::chrono::milliseconds(e.durationMs));
                ListView_SetItemText(state->list, i, 6, dur.data());
            }
        }

        CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
            700, 340, 90, 30, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCloseId)),
            GetModuleHandleW(nullptr), nullptr);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == kCloseId || LOWORD(wParam) == IDCANCEL) {
            if (state) state->closing = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        if (state) state->closing = true;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (state) state->closing = true;
        return 0;
    case WM_SIZE: {
        if (!state || !state->list) break;
        RECT rc; GetClientRect(hwnd, &rc);
        MoveWindow(state->list, 10, 10, rc.right - 20, rc.bottom - 60, TRUE);
        if (HWND btn = GetDlgItem(hwnd, kCloseId)) {
            MoveWindow(btn, rc.right - 110, rc.bottom - 40, 90, 30, TRUE);
        }
        return 0;
    }
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, theme::Dark().text);
        SetBkColor(hdc, theme::Dark().panel);
        return reinterpret_cast<LRESULT>(theme::Brush(theme::Dark().panel));
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void EnsureHistoryClass() {
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = HistoryWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = theme::Brush(theme::Dark().panel);
    wc.lpszClassName = kHistoryClass;
    RegisterClassExW(&wc);
    registered = true;
}

} // namespace

void HistoryDialog::Show(HWND parent, HistoryDatabase& db) {
    EnsureHistoryClass();

    HistoryState state;
    state.db = &db;

    RECT prc{};
    GetWindowRect(parent, &prc);
    const int w = 820, h = 420;
    const int x = prc.left + ((prc.right - prc.left) - w) / 2;
    const int y = prc.top + ((prc.bottom - prc.top) - h) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, kHistoryClass, L"Recovery History",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_VISIBLE,
        x, y, w, h, parent, nullptr, GetModuleHandleW(nullptr), &state);
    if (!hwnd) return;

    EnableWindow(parent, FALSE);
    SetForegroundWindow(hwnd);

    MSG msg;
    while (!state.closing && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsWindow(hwnd)) break;
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (IsWindow(hwnd)) DestroyWindow(hwnd);
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
}

} // namespace pcdr
