#pragma once

#include "ui/DrivePanel.hpp"
#include "ui/ResultsList.hpp"
#include "ui/PreviewPanel.hpp"
#include "ui/ProgressBar.hpp"
#include "scanner/ScanEngine.hpp"
#include "scanner/ResultStore.hpp"
#include "recovery/RecoveryEngine.hpp"
#include "preview/PreviewEngine.hpp"
#include "database/HistoryDatabase.hpp"
#include "drive/VolumeReader.hpp"

#include <windows.h>
#include <memory>

namespace pcdr {

class AppWindow {
public:
    AppWindow();
    ~AppWindow();

    bool Create(HINSTANCE instance);
    int Run();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void OnCreate();
    void OnSize();
    void OnCommand(int id, int notifyCode = 0);
    void OnNotify(NMHDR* hdr);
    void RefreshResultsView();
    void StartScan();
    void RecoverSelected();
    void UpdatePreviewForSelection();
    void ShowSafetyWarning();
    void MarkResultsDirty();

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;

    DrivePanel drivePanel_;
    ResultsList resultsList_;
    PreviewPanel previewPanel_;
    ProgressBarPanel progressBar_;

    HWND modeTitle_ = nullptr;
    HWND radioQuick_ = nullptr;
    HWND radioDeep_ = nullptr;
    HWND radioRaw_ = nullptr;
    HWND controlsTitle_ = nullptr;
    HWND btnStart_ = nullptr;
    HWND btnPause_ = nullptr;
    HWND btnResume_ = nullptr;
    HWND btnCancel_ = nullptr;
    HWND btnRecover_ = nullptr;
    HWND btnSelectAll_ = nullptr;
    HWND btnSettings_ = nullptr;
    HWND btnHistory_ = nullptr;
    HWND topTitle_ = nullptr;
    HWND safetyNote_ = nullptr;

    ScanEngine scanEngine_;
    ResultStore results_;
    HistoryDatabase history_;
    PreviewEngine previewEngine_;
    std::unique_ptr<VolumeReader> previewReader_;
    DriveInfo currentDrive_{};
    ScanMode currentMode_ = ScanMode::Quick;
    bool resultsDirty_ = false;
    DWORD lastProgressUiTick_ = 0;
};

} // namespace pcdr
