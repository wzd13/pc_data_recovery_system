#include "ui/ResultsList.hpp"
#include "ui/Controls.hpp"
#include "ui/Theme.hpp"
#include "utils/StringUtil.hpp"
#include "utils/TimeUtil.hpp"

#include <commctrl.h>

namespace pcdr {

void ResultsList::Create(HWND parent, int idList, int idSearch, int idType, int idConf, int idSort) {
    parent_ = parent;
    label_ = ui::CreateDarkStatic(parent, -1, L"Scan Results", 0, 0, 200, 22);
    SendMessageW(label_, WM_SETFONT, reinterpret_cast<WPARAM>(theme::TitleFont()), TRUE);

    search_ = ui::CreateDarkEdit(parent, idSearch, 0, 0, 100, 24);
    SetWindowTextW(search_, L"");
    // placeholder via cue banner
    SendMessageW(search_, 0x1501 /*EM_SETCUEBANNER*/, TRUE, reinterpret_cast<LPARAM>(L"Search filename..."));

    type_ = ui::CreateDarkCombo(parent, idType, 0, 0, 100, 200);
    SendMessageW(type_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"All types"));
    const wchar_t* types[] = { L"jpg", L"png", L"gif", L"bmp", L"tif", L"mp4", L"mov", L"avi", L"mkv",
        L"mp3", L"wav", L"pdf", L"doc", L"docx", L"xls", L"xlsx", L"ppt", L"pptx", L"zip", L"rar", L"7z",
        L"iso", L"sqlite", L"txt", L"csv" };
    for (auto* t : types) SendMessageW(type_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t));
    SendMessageW(type_, CB_SETCURSEL, 0, 0);

    conf_ = ui::CreateDarkCombo(parent, idConf, 0, 0, 100, 200);
    SendMessageW(conf_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"All confidence"));
    SendMessageW(conf_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Excellent"));
    SendMessageW(conf_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Good"));
    SendMessageW(conf_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Partial"));
    SendMessageW(conf_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Corrupted"));
    SendMessageW(conf_, CB_SETCURSEL, 0, 0);

    sort_ = ui::CreateDarkCombo(parent, idSort, 0, 0, 100, 200);
    SendMessageW(sort_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Sort: Name"));
    SendMessageW(sort_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Sort: Size"));
    SendMessageW(sort_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Sort: Date"));
    SendMessageW(sort_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Sort: Type"));
    SendMessageW(sort_, CB_SETCURSEL, 0, 0);

    list_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
        0, 0, 100, 100, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(idList)),
        GetModuleHandleW(nullptr), nullptr);
    ListView_SetExtendedListViewStyle(list_, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(list_, theme::Dark().panel);
    ListView_SetTextBkColor(list_, theme::Dark().panel);
    ListView_SetTextColor(list_, theme::Dark().text);

    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = const_cast<LPWSTR>(L"Name"); col.cx = 180; ListView_InsertColumn(list_, 0, &col);
    col.pszText = const_cast<LPWSTR>(L"Ext"); col.cx = 50; ListView_InsertColumn(list_, 1, &col);
    col.pszText = const_cast<LPWSTR>(L"Path"); col.cx = 180; ListView_InsertColumn(list_, 2, &col);
    col.pszText = const_cast<LPWSTR>(L"Size"); col.cx = 80; ListView_InsertColumn(list_, 3, &col);
    col.pszText = const_cast<LPWSTR>(L"Type"); col.cx = 90; ListView_InsertColumn(list_, 4, &col);
    col.pszText = const_cast<LPWSTR>(L"Date"); col.cx = 120; ListView_InsertColumn(list_, 5, &col);
    col.pszText = const_cast<LPWSTR>(L"Status"); col.cx = 80; ListView_InsertColumn(list_, 6, &col);
    col.pszText = const_cast<LPWSTR>(L"Confidence"); col.cx = 90; ListView_InsertColumn(list_, 7, &col);
    col.pszText = const_cast<LPWSTR>(L"Found By"); col.cx = 90; ListView_InsertColumn(list_, 8, &col);
}

void ResultsList::Layout(RECT rc) {
    const int top = rc.top;
    MoveWindow(label_, rc.left, top, 200, 24, TRUE);
    MoveWindow(search_, rc.left, top + 30, 180, 24, TRUE);
    MoveWindow(type_, rc.left + 190, top + 30, 110, 200, TRUE);
    MoveWindow(conf_, rc.left + 310, top + 30, 130, 200, TRUE);
    MoveWindow(sort_, rc.left + 450, top + 30, 130, 200, TRUE);
    MoveWindow(list_, rc.left, top + 62, rc.right - rc.left, rc.bottom - (top + 62), TRUE);
}

void ResultsList::SetFiles(const std::vector<RecoveredFile>& files) {
    view_ = files;
    RebuildView();
}

ResultFilter ResultsList::CurrentFilter() const {
    ResultFilter f;
    wchar_t buf[512]{};
    GetWindowTextW(search_, buf, 512);
    f.searchName = buf;

    const int t = static_cast<int>(SendMessageW(type_, CB_GETCURSEL, 0, 0));
    if (t > 0) {
        wchar_t ext[64]{};
        SendMessageW(type_, CB_GETLBTEXT, t, reinterpret_cast<LPARAM>(ext));
        f.extension = ext;
    }
    const int c = static_cast<int>(SendMessageW(conf_, CB_GETCURSEL, 0, 0));
    if (c == 1) f.confidence = Confidence::Excellent;
    else if (c == 2) f.confidence = Confidence::Good;
    else if (c == 3) f.confidence = Confidence::Partial;
    else if (c == 4) f.confidence = Confidence::Corrupted;
    return f;
}

SortKey ResultsList::CurrentSort() const {
    const int s = static_cast<int>(SendMessageW(sort_, CB_GETCURSEL, 0, 0));
    if (s == 1) return SortKey::Size;
    if (s == 2) return SortKey::Date;
    if (s == 3) return SortKey::Type;
    return SortKey::Name;
}

void ResultsList::ApplyFilter() {
    // Parent will re-query store; this just rebuilds current view_ if already filtered externally.
    RebuildView();
}

void ResultsList::RebuildView() {
    ListView_DeleteAllItems(list_);
    for (int i = 0; i < static_cast<int>(view_.size()); ++i) {
        auto& f = view_[i];
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = i;
        item.lParam = static_cast<LPARAM>(f.id);
        item.pszText = f.displayName.data();
        ListView_InsertItem(list_, &item);
        ListView_SetItemText(list_, i, 1, f.extension.data());
        ListView_SetItemText(list_, i, 2, f.originalPath.data());
        std::wstring size = str::FormatBytes(f.sizeBytes);
        ListView_SetItemText(list_, i, 3, size.data());
        ListView_SetItemText(list_, i, 4, f.typeLabel.data());
        std::wstring date = f.modifiedUtc ? timeutil::FormatUnixLocal(*f.modifiedUtc) : L"—";
        ListView_SetItemText(list_, i, 5, date.data());
        std::wstring status = ToString(f.status);
        ListView_SetItemText(list_, i, 6, status.data());
        std::wstring conf = ToString(f.confidence);
        ListView_SetItemText(list_, i, 7, conf.data());
        std::wstring method = ToString(f.method);
        ListView_SetItemText(list_, i, 8, method.data());
    }
}

std::vector<std::uint64_t> ResultsList::SelectedIds() const {
    std::vector<std::uint64_t> ids;
    const int count = ListView_GetItemCount(list_);
    for (int i = 0; i < count; ++i) {
        if (ListView_GetCheckState(list_, i)) {
            LVITEMW item{};
            item.mask = LVIF_PARAM;
            item.iItem = i;
            ListView_GetItem(list_, &item);
            ids.push_back(static_cast<std::uint64_t>(item.lParam));
        }
    }
    // Also include highlighted selection if nothing checked
    if (ids.empty()) {
        int i = -1;
        while ((i = ListView_GetNextItem(list_, i, LVNI_SELECTED)) != -1) {
            LVITEMW item{};
            item.mask = LVIF_PARAM;
            item.iItem = i;
            ListView_GetItem(list_, &item);
            ids.push_back(static_cast<std::uint64_t>(item.lParam));
        }
    }
    return ids;
}

void ResultsList::SelectAll() {
    const int count = ListView_GetItemCount(list_);
    for (int i = 0; i < count; ++i) ListView_SetCheckState(list_, i, TRUE);
}

std::optional<std::uint64_t> ResultsList::FocusedId() const {
    const int i = ListView_GetNextItem(list_, -1, LVNI_FOCUSED | LVNI_SELECTED);
    if (i < 0) return std::nullopt;
    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = i;
    ListView_GetItem(list_, &item);
    return static_cast<std::uint64_t>(item.lParam);
}

} // namespace pcdr
