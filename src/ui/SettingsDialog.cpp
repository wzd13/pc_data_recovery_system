#include "ui/SettingsDialog.hpp"
#include "settings/Settings.hpp"
#include "ui/Theme.hpp"
#include "utils/FileUtil.hpp"

#include <shlobj.h>

namespace pcdr {
namespace {

constexpr wchar_t kSettingsClass[] = L"PCDataRecoverySettingsWnd";
constexpr int kOk = 1;
constexpr int kCancel = 2;
constexpr int kBrowse = 3;
constexpr int kFolder = 10;
constexpr int kPreview = 11;
constexpr int kMaxSize = 12;

struct SettingsState {
    bool closing = false;
    bool saved = false;
    HWND folder = nullptr;
};

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<SettingsState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = reinterpret_cast<SettingsState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        auto& s = Settings::Instance().Get();
        CreateWindowW(L"STATIC", L"Default recovery folder:", WS_CHILD | WS_VISIBLE,
            16, 20, 200, 18, hwnd, nullptr, nullptr, nullptr);
        state->folder = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", s.defaultRecoveryFolder.c_str(),
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
            16, 42, 320, 24, hwnd, reinterpret_cast<HMENU>(kFolder), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            346, 40, 90, 28, hwnd, reinterpret_cast<HMENU>(kBrowse), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Enable preview", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            16, 84, 200, 22, hwnd, reinterpret_cast<HMENU>(kPreview), nullptr, nullptr);
        CheckDlgButton(hwnd, kPreview, s.enablePreview ? BST_CHECKED : BST_UNCHECKED);
        CreateWindowW(L"STATIC", L"Maximum file size (MB):", WS_CHILD | WS_VISIBLE,
            16, 120, 180, 18, hwnd, nullptr, nullptr, nullptr);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            std::to_wstring(s.maxFileSize / (1024 * 1024)).c_str(),
            WS_CHILD | WS_VISIBLE | ES_NUMBER | WS_TABSTOP,
            200, 116, 100, 24, hwnd, reinterpret_cast<HMENU>(kMaxSize), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
            250, 180, 90, 30, hwnd, reinterpret_cast<HMENU>(kOk), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            350, 180, 90, 30, hwnd, reinterpret_cast<HMENU>(kCancel), nullptr, nullptr);
        return 0;
    }
    case WM_COMMAND: {
        if (!state) break;
        const int id = LOWORD(wParam);
        if (id == kBrowse) {
            wchar_t path[MAX_PATH]{};
            BROWSEINFOW bi{};
            bi.hwndOwner = hwnd;
            bi.lpszTitle = L"Select default recovery folder";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            if (LPITEMIDLIST pidl = SHBrowseForFolderW(&bi)) {
                SHGetPathFromIDListW(pidl, path);
                CoTaskMemFree(pidl);
                SetWindowTextW(state->folder, path);
            }
            return 0;
        }
        if (id == kOk) {
            auto& s = Settings::Instance().Get();
            wchar_t buf[MAX_PATH]{};
            GetWindowTextW(state->folder, buf, MAX_PATH);
            s.defaultRecoveryFolder = buf;
            s.enablePreview = IsDlgButtonChecked(hwnd, kPreview) == BST_CHECKED;
            wchar_t mb[64]{};
            GetDlgItemTextW(hwnd, kMaxSize, mb, 64);
            try { s.maxFileSize = std::stoull(mb) * 1024ULL * 1024ULL; } catch (...) {}
            fileutil::EnsureDirectory(s.defaultRecoveryFolder);
            Settings::Instance().Save();
            state->saved = true;
            state->closing = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == kCancel || id == IDCANCEL) {
            state->closing = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        if (state) state->closing = true;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (state) state->closing = true;
        return 0;
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, theme::Dark().text);
        SetBkColor(hdc, theme::Dark().panel);
        return reinterpret_cast<LRESULT>(theme::Brush(theme::Dark().panel));
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void EnsureSettingsClass() {
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = theme::Brush(theme::Dark().panel);
    wc.lpszClassName = kSettingsClass;
    RegisterClassExW(&wc);
    registered = true;
}

} // namespace

void SettingsDialog::Show(HWND parent) {
    EnsureSettingsClass();
    SettingsState state;

    RECT prc{};
    GetWindowRect(parent, &prc);
    const int w = 460, h = 260;
    const int x = prc.left + ((prc.right - prc.left) - w) / 2;
    const int y = prc.top + ((prc.bottom - prc.top) - h) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, kSettingsClass, L"Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, w, h, parent, nullptr, GetModuleHandleW(nullptr), &state);
    if (!hwnd) {
        MessageBoxW(parent, L"Unable to open settings.", L"Settings", MB_ICONERROR);
        return;
    }

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
