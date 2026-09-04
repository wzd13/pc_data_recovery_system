#include "ui/DrivePanel.hpp"
#include "ui/Controls.hpp"
#include "ui/Theme.hpp"
#include "drive/DriveDetector.hpp"
#include "utils/StringUtil.hpp"

#include <commctrl.h>

namespace pcdr {

void DrivePanel::Create(HWND parent, int idList, int idRefresh) {
    parent_ = parent;
    title_ = ui::CreateDarkStatic(parent, -1, L"Drives", 0, 0, 100, 20);
    SendMessageW(title_, WM_SETFONT, reinterpret_cast<WPARAM>(theme::TitleFont()), TRUE);

    list_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 100, 100, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(idList)),
        GetModuleHandleW(nullptr), nullptr);
    ListView_SetExtendedListViewStyle(list_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(list_, theme::Dark().panel);
    ListView_SetTextBkColor(list_, theme::Dark().panel);
    ListView_SetTextColor(list_, theme::Dark().text);

    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = const_cast<LPWSTR>(L"Drive"); col.cx = 50; ListView_InsertColumn(list_, 0, &col);
    col.pszText = const_cast<LPWSTR>(L"Name"); col.cx = 90; ListView_InsertColumn(list_, 1, &col);
    col.pszText = const_cast<LPWSTR>(L"FS"); col.cx = 55; ListView_InsertColumn(list_, 2, &col);
    col.pszText = const_cast<LPWSTR>(L"Free / Total"); col.cx = 130; ListView_InsertColumn(list_, 3, &col);
    col.pszText = const_cast<LPWSTR>(L"Type"); col.cx = 110; ListView_InsertColumn(list_, 4, &col);

    refresh_ = ui::CreateDarkButton(parent, idRefresh, L"Refresh Drives", 0, 0, 120, 28);
    Refresh();
}

void DrivePanel::Layout(RECT rc) {
    MoveWindow(title_, rc.left, rc.top, rc.right - rc.left, 24, TRUE);
    MoveWindow(refresh_, rc.left, rc.bottom - 32, 130, 28, TRUE);
    MoveWindow(list_, rc.left, rc.top + 28, rc.right - rc.left, (rc.bottom - rc.top) - 68, TRUE);
}

void DrivePanel::Refresh() {
    DriveDetector detector;
    drives_ = detector.EnumerateDrives();
    ListView_DeleteAllItems(list_);
    for (int i = 0; i < static_cast<int>(drives_.size()); ++i) {
        const auto& d = drives_[i];
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = i;
        item.lParam = i;
        std::wstring letter = d.letter;
        item.pszText = letter.data();
        ListView_InsertItem(list_, &item);

        ListView_SetItemText(list_, i, 1, const_cast<LPWSTR>(d.volumeName.empty() ? L"(No Label)" : d.volumeName.c_str()));
        ListView_SetItemText(list_, i, 2, const_cast<LPWSTR>(d.fileSystemName.c_str()));
        std::wstring space = str::FormatBytes(d.freeBytes) + L" / " + str::FormatBytes(d.totalBytes);
        ListView_SetItemText(list_, i, 3, space.data());
        std::wstring media = ToString(d.mediaType);
        if (!d.physicalDiskModel.empty()) media += L" — " + d.physicalDiskModel;
        ListView_SetItemText(list_, i, 4, media.data());
    }
}

std::optional<DriveInfo> DrivePanel::Selected() const {
    const int sel = ListView_GetNextItem(list_, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= static_cast<int>(drives_.size())) return std::nullopt;
    return drives_[sel];
}

} // namespace pcdr
