#include "ui/AppWindow.hpp"
#include "ui/Controls.hpp"
#include "ui/Theme.hpp"
#include "ui/ResourceIds.hpp"
#include "ui/SettingsDialog.hpp"
#include "ui/HistoryDialog.hpp"
#include "settings/Settings.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtil.hpp"
#include "utils/PathUtil.hpp"
#include "utils/FileUtil.hpp"

#include <commctrl.h>
#include <dwmapi.h>
#include <shlobj.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

namespace pcdr {
namespace {
AppWindow* g_app = nullptr;

constexpr int kLeftW = 320;
constexpr int kRightW = 300;
constexpr int kTopH = 52;
constexpr int kBottomH = 58;
}

AppWindow::AppWindow() { g_app = this; }
AppWindow::~AppWindow() {
    theme::Shutdown();
    if (g_app == this) g_app = nullptr;
}

bool AppWindow::Create(HINSTANCE instance) {
    instance_ = instance;
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = theme::Brush(theme::Dark().background);
    wc.lpszClassName = L"PCDataRecoveryMainWindow";
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) return false;

    hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"PC Data Recovery",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
        nullptr, nullptr, instance, this);

    if (!hwnd_) return false;

    // Dark title bar on supported Windows
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd_, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

int AppWindow::Run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK AppWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<AppWindow*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT AppWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        OnCreate();
        return 0;
    case WM_SIZE:
        OnSize();
        return 0;
    case WM_COMMAND:
        OnCommand(LOWORD(wParam), HIWORD(wParam));
        return 0;
    case WM_NOTIFY:
        OnNotify(reinterpret_cast<NMHDR*>(lParam));
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
        return ui::HandleCtlColor(wParam, lParam);
    case WM_ERASEBKGND: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc; GetClientRect(hwnd_, &rc);
        FillRect(hdc, &rc, theme::Brush(theme::Dark().background));
        return 1;
    }
    case WM_APP_SCAN_PROGRESS: {
        auto* stats = reinterpret_cast<ScanStats*>(lParam);
        if (stats) {
            const DWORD now = GetTickCount();
            if (now - lastProgressUiTick_ >= 150 ||
                stats->state == ScanState::Completed ||
                stats->state == ScanState::Failed ||
                stats->state == ScanState::Idle) {
                progressBar_.Update(*stats);
                lastProgressUiTick_ = now;
            }
            delete stats;
        }
        return 0;
    }
    case WM_APP_SCAN_FILE: {
        MarkResultsDirty();
        return 0;
    }
    case WM_APP_SCAN_DONE: {
        resultsDirty_ = false;
        RefreshResultsView();
        auto stats = scanEngine_.GetStats();
        progressBar_.Update(stats);
        EnableWindow(btnStart_, TRUE);
        EnableWindow(btnPause_, FALSE);
        EnableWindow(btnResume_, FALSE);
        EnableWindow(btnCancel_, FALSE);
        KillTimer(hwnd_, IDT_RESULTS_REFRESH);
        return 0;
    }
    case WM_TIMER:
        if (wParam == IDT_RESULTS_REFRESH && resultsDirty_) {
            resultsDirty_ = false;
            RefreshResultsView();
        }
        return 0;
    case WM_DESTROY:
        scanEngine_.Cancel();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

void AppWindow::OnCreate() {
    Settings::Instance().Load();
    history_.Open();
    Logger::Instance().SetLogDirectory(pathutil::GetLogsDirectory());
    Logger::Instance().Info(L"PC Data Recovery started");

    topTitle_ = ui::CreateDarkStatic(hwnd_, -1, L"PC Data Recovery", 0, 0, 300, 28);
    SendMessageW(topTitle_, WM_SETFONT, reinterpret_cast<WPARAM>(theme::TitleFont()), TRUE);
    btnSettings_ = ui::CreateDarkButton(hwnd_, IDC_BTN_SETTINGS, L"Settings", 0, 0, 90, 28);
    btnHistory_ = ui::CreateDarkButton(hwnd_, IDC_BTN_HISTORY, L"History", 0, 0, 90, 28);

    drivePanel_.Create(hwnd_, IDC_DRIVE_LIST, IDC_BTN_REFRESH);

    modeTitle_ = ui::CreateDarkStatic(hwnd_, -1, L"Scan Modes", 0, 0, 120, 20);
    SendMessageW(modeTitle_, WM_SETFONT, reinterpret_cast<WPARAM>(theme::TitleFont()), TRUE);
    radioQuick_ = CreateWindowW(L"BUTTON", L"Quick Scan — filesystem deleted records",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 0, 0, 280, 22, hwnd_,
        reinterpret_cast<HMENU>(IDC_SCAN_QUICK), instance_, nullptr);
    radioDeep_ = CreateWindowW(L"BUTTON", L"Deep Scan — thorough filesystem scan",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 0, 0, 280, 22, hwnd_,
        reinterpret_cast<HMENU>(IDC_SCAN_DEEP), instance_, nullptr);
    radioRaw_ = CreateWindowW(L"BUTTON", L"RAW Recovery — file signature carving",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 0, 0, 280, 22, hwnd_,
        reinterpret_cast<HMENU>(IDC_SCAN_RAW), instance_, nullptr);
    ui::SetControlFont(radioQuick_);
    ui::SetControlFont(radioDeep_);
    ui::SetControlFont(radioRaw_);
    CheckRadioButton(hwnd_, IDC_SCAN_QUICK, IDC_SCAN_RAW, IDC_SCAN_QUICK);

    controlsTitle_ = ui::CreateDarkStatic(hwnd_, -1, L"Scan Controls", 0, 0, 140, 20);
    SendMessageW(controlsTitle_, WM_SETFONT, reinterpret_cast<WPARAM>(theme::TitleFont()), TRUE);
    btnStart_ = ui::CreateDarkButton(hwnd_, IDC_BTN_START, L"Start", 0, 0, 70, 28);
    btnPause_ = ui::CreateDarkButton(hwnd_, IDC_BTN_PAUSE, L"Pause", 0, 0, 70, 28);
    btnResume_ = ui::CreateDarkButton(hwnd_, IDC_BTN_RESUME, L"Resume", 0, 0, 70, 28);
    btnCancel_ = ui::CreateDarkButton(hwnd_, IDC_BTN_CANCEL, L"Cancel", 0, 0, 70, 28);
    EnableWindow(btnPause_, FALSE);
    EnableWindow(btnResume_, FALSE);
    EnableWindow(btnCancel_, FALSE);

    safetyNote_ = ui::CreateDarkStatic(hwnd_, -1,
        L"Safety: source volumes are opened read-only. Do not recover onto the source drive.",
        0, 0, 300, 40);

    resultsList_.Create(hwnd_, IDC_RESULTS_LIST, IDC_SEARCH_EDIT, IDC_FILTER_TYPE, IDC_FILTER_CONF, IDC_SORT_COMBO);
    btnSelectAll_ = ui::CreateDarkButton(hwnd_, IDC_BTN_SELECT_ALL, L"Select All", 0, 0, 90, 28);
    btnRecover_ = ui::CreateDarkButton(hwnd_, IDC_BTN_RECOVER, L"Recover Selected...", 0, 0, 150, 28);

    previewPanel_.Create(hwnd_, IDC_PREVIEW_HOST);
    progressBar_.Create(hwnd_, IDC_STATUS);

    scanEngine_.SetProgressCallback([this](const ScanStats& s) {
        auto* heap = new ScanStats(s);
        if (!PostMessageW(hwnd_, WM_APP_SCAN_PROGRESS, 0, reinterpret_cast<LPARAM>(heap))) {
            delete heap;
        }
        if (s.state == ScanState::Completed || s.state == ScanState::Failed || s.state == ScanState::Idle) {
            PostMessageW(hwnd_, WM_APP_SCAN_DONE, 0, 0);
        }
    });
    scanEngine_.SetFileFoundCallback([this](const RecoveredFile&) {
        PostMessageW(hwnd_, WM_APP_SCAN_FILE, 0, 0);
    });

    ShowSafetyWarning();
    OnSize();
}

void AppWindow::MarkResultsDirty() {
    resultsDirty_ = true;
}

void AppWindow::ShowSafetyWarning() {
    MessageBoxW(hwnd_,
        L"IMPORTANT SAFETY NOTICE\n\n"
        L"• This application opens storage devices in READ-ONLY mode whenever possible.\n"
        L"• Scanning never modifies the source drive.\n"
        L"• Continued use of a drive with deleted/lost files may overwrite recoverable data.\n"
        L"• Always recover files to a DIFFERENT drive or folder.\n"
        L"• Administrator rights may be required for raw volume access.\n\n"
        L"PC Data Recovery does not invent results — unsupported recoveries are labeled clearly.",
        L"PC Data Recovery — Safety",
        MB_OK | MB_ICONWARNING);
}

void AppWindow::OnSize() {
    RECT rc; GetClientRect(hwnd_, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;

    MoveWindow(topTitle_, 16, 12, 360, 28, TRUE);
    MoveWindow(btnHistory_, w - 210, 12, 90, 28, TRUE);
    MoveWindow(btnSettings_, w - 110, 12, 90, 28, TRUE);

    RECT left{12, kTopH, 12 + kLeftW, h - kBottomH};
    // Drive list upper portion
    RECT driveRc = left;
    driveRc.bottom = left.top + (left.bottom - left.top) / 2;
    drivePanel_.Layout(driveRc);

    MoveWindow(modeTitle_, left.left, driveRc.bottom + 8, 200, 22, TRUE);
    MoveWindow(radioQuick_, left.left, driveRc.bottom + 34, kLeftW - 8, 22, TRUE);
    MoveWindow(radioDeep_, left.left, driveRc.bottom + 58, kLeftW - 8, 22, TRUE);
    MoveWindow(radioRaw_, left.left, driveRc.bottom + 82, kLeftW - 8, 22, TRUE);

    MoveWindow(controlsTitle_, left.left, driveRc.bottom + 116, 200, 22, TRUE);
    MoveWindow(btnStart_, left.left, driveRc.bottom + 142, 70, 28, TRUE);
    MoveWindow(btnPause_, left.left + 78, driveRc.bottom + 142, 70, 28, TRUE);
    MoveWindow(btnResume_, left.left + 156, driveRc.bottom + 142, 70, 28, TRUE);
    MoveWindow(btnCancel_, left.left + 234, driveRc.bottom + 142, 70, 28, TRUE);
    MoveWindow(safetyNote_, left.left, driveRc.bottom + 180, kLeftW - 8, 50, TRUE);

    RECT center{12 + kLeftW + 12, kTopH, w - kRightW - 24, h - kBottomH - 36};
    resultsList_.Layout(center);
    MoveWindow(btnSelectAll_, center.left, h - kBottomH - 32, 90, 28, TRUE);
    MoveWindow(btnRecover_, center.left + 100, h - kBottomH - 32, 160, 28, TRUE);

    RECT right{w - kRightW - 12, kTopH, w - 12, h - kBottomH};
    previewPanel_.Layout(right);

    RECT bottom{12, h - kBottomH + 4, w - 12, h - 4};
    progressBar_.Layout(bottom);
}

void AppWindow::OnCommand(int id, int notifyCode) {
    switch (id) {
    case IDC_BTN_REFRESH:
        drivePanel_.Refresh();
        progressBar_.SetStatus(L"Drive list refreshed.");
        break;
    case IDC_SCAN_QUICK: currentMode_ = ScanMode::Quick; break;
    case IDC_SCAN_DEEP: currentMode_ = ScanMode::Deep; break;
    case IDC_SCAN_RAW: currentMode_ = ScanMode::Raw; break;
    case IDC_BTN_START: StartScan(); break;
    case IDC_BTN_PAUSE:
        scanEngine_.Pause();
        EnableWindow(btnPause_, FALSE);
        EnableWindow(btnResume_, TRUE);
        break;
    case IDC_BTN_RESUME:
        scanEngine_.Resume();
        EnableWindow(btnPause_, TRUE);
        EnableWindow(btnResume_, FALSE);
        break;
    case IDC_BTN_CANCEL:
        scanEngine_.Cancel();
        break;
    case IDC_BTN_SELECT_ALL:
        resultsList_.SelectAll();
        break;
    case IDC_BTN_RECOVER:
        RecoverSelected();
        break;
    case IDC_BTN_SETTINGS:
        SettingsDialog::Show(hwnd_);
        break;
    case IDC_BTN_HISTORY:
        HistoryDialog::Show(hwnd_, history_);
        break;
    case IDC_FILTER_TYPE:
    case IDC_FILTER_CONF:
    case IDC_SORT_COMBO:
        if (notifyCode == CBN_SELCHANGE) RefreshResultsView();
        break;
    case IDC_SEARCH_EDIT:
        if (notifyCode == EN_CHANGE) RefreshResultsView();
        break;
    }
}

void AppWindow::OnNotify(NMHDR* hdr) {
    if (!hdr) return;
    if (hdr->idFrom == IDC_RESULTS_LIST && hdr->code == LVN_ITEMCHANGED) {
        UpdatePreviewForSelection();
    }
    if (hdr->idFrom == IDC_DRIVE_LIST && hdr->code == LVN_ITEMCHANGED) {
        auto drive = drivePanel_.Selected();
        if (drive) {
            std::wstring msg = L"Selected " + drive->letter + L" — " + ToString(drive->mediaType) +
                L" | " + drive->fileSystemName + L" | " + str::FormatBytes(drive->totalBytes);
            if (!drive->physicalDiskModel.empty()) msg += L" | Disk: " + drive->physicalDiskModel;
            progressBar_.SetStatus(msg);
        }
    }
}

void AppWindow::RefreshResultsView() {
    auto files = results_.Query(resultsList_.CurrentFilter(), resultsList_.CurrentSort(), true);
    resultsList_.SetFiles(files);
}

void AppWindow::StartScan() {
    auto drive = drivePanel_.Selected();
    if (!drive) {
        MessageBoxW(hwnd_, L"Select a drive to scan.", L"Scan", MB_ICONINFORMATION);
        return;
    }

    if (IsDlgButtonChecked(hwnd_, IDC_SCAN_DEEP) == BST_CHECKED) currentMode_ = ScanMode::Deep;
    else if (IsDlgButtonChecked(hwnd_, IDC_SCAN_RAW) == BST_CHECKED) currentMode_ = ScanMode::Raw;
    else currentMode_ = ScanMode::Quick;

    std::wstring warn = L"You are about to scan " + drive->letter + L" (" + drive->volumeName + L")\n"
        L"Mode: " + std::wstring(ToString(currentMode_)) + L"\n\n"
        L"The source will be opened READ-ONLY.\n"
        L"Avoid writing to this drive while scanning.\n";
    if (currentMode_ == ScanMode::Raw) {
        warn += L"\nRAW Recovery scans the whole volume and can take many hours on large drives.\n"
                L"You can Pause or Cancel at any time.\n";
    }
    warn += L"\nContinue?";
    if (MessageBoxW(hwnd_, warn.c_str(), L"Start Scan", MB_OKCANCEL | MB_ICONWARNING) != IDOK) return;

    currentDrive_ = *drive;
    previewReader_.reset();
    previewPanel_.Clear();
    resultsDirty_ = false;

    ScanOptions options;
    options.mode = currentMode_;
    options.maxFileSize = Settings::Instance().Get().maxFileSize;
    options.includeHidden = Settings::Instance().Get().includeHidden;
    options.includeSystem = Settings::Instance().Get().includeSystem;
    options.enabledExtensions = Settings::Instance().Get().enabledExtensions;

    if (!scanEngine_.Start(currentDrive_, options, results_)) {
        MessageBoxW(hwnd_, L"A scan is already running.", L"Scan", MB_ICONINFORMATION);
        return;
    }

    SetTimer(hwnd_, IDT_RESULTS_REFRESH, 500, nullptr);
    EnableWindow(btnStart_, FALSE);
    EnableWindow(btnPause_, TRUE);
    EnableWindow(btnResume_, FALSE);
    EnableWindow(btnCancel_, TRUE);
    progressBar_.SetStatus(L"Starting scan...");
}

void AppWindow::RecoverSelected() {
    auto ids = resultsList_.SelectedIds();
    if (ids.empty()) {
        MessageBoxW(hwnd_, L"Select one or more files to recover (use checkboxes).", L"Recover", MB_ICONINFORMATION);
        return;
    }
    if (currentDrive_.letter.empty()) {
        MessageBoxW(hwnd_, L"No source drive context. Run a scan first.", L"Recover", MB_ICONWARNING);
        return;
    }

    auto files = results_.GetByIds(ids);
    RecoveryOptions opt;
    opt.destinationFolder = Settings::Instance().Get().defaultRecoveryFolder;

    // Browse for destination
    wchar_t path[MAX_PATH]{};
    BROWSEINFOW bi{};
    bi.hwndOwner = hwnd_;
    bi.lpszTitle = L"Choose recovery destination (must NOT be the source drive)";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    if (LPITEMIDLIST pidl = SHBrowseForFolderW(&bi)) {
        SHGetPathFromIDListW(pidl, path);
        CoTaskMemFree(pidl);
        opt.destinationFolder = path;
    } else {
        return;
    }

    if (RecoveryEngine::IsDestinationOnSource(opt.destinationFolder, currentDrive_.letter)) {
        MessageBoxW(hwnd_,
            L"Recovery to the source drive is blocked for safety.\n"
            L"Choose a different drive or folder.",
            L"Safety Block", MB_ICONERROR);
        return;
    }

    std::wstring msg = L"Recover " + std::to_wstring(files.size()) + L" file(s)\n"
        L"From: " + currentDrive_.letter + L"\n"
        L"To: " + opt.destinationFolder + L"\n\n"
        L"Continue?";
    if (MessageBoxW(hwnd_, msg.c_str(), L"Confirm Recovery", MB_OKCANCEL | MB_ICONWARNING) != IDOK) return;

    RecoveryEngine engine;
    std::wstring error;
    progressBar_.SetStatus(L"Recovering files...");
    const bool allOk = engine.Recover(currentDrive_, files, opt, results_, history_,
        [this](const RecoveryItemResult& item, std::size_t done, std::size_t total) {
            std::wstring s = L"Recovering " + std::to_wstring(done) + L"/" + std::to_wstring(total) +
                L" — " + item.fileName + (item.success ? L" OK" : L" FAILED");
            progressBar_.SetStatus(s);
        }, error);

    RefreshResultsView();
    if (!error.empty() && !allOk && files.empty()) {
        MessageBoxW(hwnd_, error.c_str(), L"Recovery", MB_ICONERROR);
    } else {
        MessageBoxW(hwnd_,
            allOk ? L"Recovery completed successfully.\nA report was saved in the destination folder."
                  : (error.empty()
                        ? L"Recovery finished with some failures.\nSee the destination report for details."
                        : error.c_str()),
            L"Recovery", allOk ? MB_ICONINFORMATION : MB_ICONWARNING);
    }
}

void AppWindow::UpdatePreviewForSelection() {
    auto id = resultsList_.FocusedId();
    if (!id) return;
    auto file = results_.Get(*id);
    if (!file) return;

    if (!previewReader_ || previewReader_->DriveLetter() != currentDrive_.letter) {
        previewReader_ = std::make_unique<VolumeReader>();
        if (!currentDrive_.letter.empty()) previewReader_->Open(currentDrive_.letter);
    }

    PreviewData data;
    previewEngine_.BuildPreview(*file, previewReader_.get(), data);
    previewPanel_.Show(data);
}

} // namespace pcdr
