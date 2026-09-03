#include "ResultsWindow.h"
#include "Utf8Utils.h"
#include "resource.h"
#include "../license-detection/LocalDatabase.h"
#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

extern HINSTANCE g_hInstance;

namespace {
const int ID_RESULTS_LISTVIEW = 2001;
const int ID_RESULTS_EXPORT_ALL = 2002;
const int ID_RESULTS_EXPORT_WINDOWS = 2003;
const int ID_RESULTS_EXPORT_OFFICE = 2004;

const int kMargin = 12;
const int kButtonWidth = 150;
const int kButtonHeight = 32;
const int kButtonSpacing = 10;
const int kBottomRowHeight = kButtonHeight + kMargin * 2;

// LVS_EX_CHECKBOXES puts every row's checkbox in column 0's normal text
// area (Windows draws it automatically), so the "real" first text column
// is column 1 here.
enum Column { ColTime = 0, ColHostname, ColOS, ColWindows, ColOffice, ColCount };

const wchar_t* kColumnTitles[ColCount] = {
    L"Thời điểm kiểm tra", L"Tên máy", L"Hệ điều hành", L"Windows", L"Office",
};
const int kColumnWidths[ColCount] = {150, 150, 170, 110, 110};

} // namespace

ResultsWindow* ResultsWindow::s_instance = nullptr;

ResultsWindow::ResultsWindow()
    : m_hwnd(nullptr), m_hListView(nullptr), m_hExportAllButton(nullptr),
      m_hExportWindowsButton(nullptr), m_hExportOfficeButton(nullptr), m_hCountLabel(nullptr) {
}

void ResultsWindow::ShowOrActivate(HWND owner) {
    if (s_instance && s_instance->m_hwnd && IsWindow(s_instance->m_hwnd)) {
        s_instance->PopulateList();
        ShowWindow(s_instance->m_hwnd, SW_SHOW);
        SetForegroundWindow(s_instance->m_hwnd);
        return;
    }

    ResultsWindow* win = new ResultsWindow();
    if (!win->Create(owner)) {
        delete win;
        MessageBoxW(owner, L"Không tạo được cửa sổ kết quả kiểm tra.", L"Lỗi", MB_OK | MB_ICONERROR);
        return;
    }
    s_instance = win;
    win->PopulateList();
    ShowWindow(win->m_hwnd, SW_SHOW);
    SetForegroundWindow(win->m_hwnd);
}

bool ResultsWindow::Create(HWND owner) {
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASS wc = {};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = g_hInstance;
        wc.lpszClassName = L"LicenseCheckerResultsWindow";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hIcon = LoadIconW(g_hInstance, MAKEINTRESOURCEW(IDI_APPICON));
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        if (!RegisterClass(&wc)) {
            return false;
        }
        classRegistered = true;
    }

    m_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"LicenseCheckerResultsWindow",
        L"Kết quả kiểm tra đã lưu",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 820, 480,
        owner, NULL, g_hInstance, this
    );
    if (!m_hwnd) {
        return false;
    }

    m_hListView = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
        kMargin, kMargin, 100, 100,
        m_hwnd, (HMENU)ID_RESULTS_LISTVIEW, g_hInstance, NULL
    );
    ListView_SetExtendedListViewStyle(m_hListView, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    for (int i = 0; i < ColCount; i++) {
        col.pszText = const_cast<wchar_t*>(kColumnTitles[i]);
        col.cx = kColumnWidths[i];
        ListView_InsertColumn(m_hListView, i, &col);
    }

    m_hCountLabel = CreateWindowW(
        L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
        kMargin, 0, 300, kButtonHeight,
        m_hwnd, NULL, g_hInstance, NULL
    );

    m_hExportAllButton = CreateWindowW(
        L"BUTTON", L"Xuất tất cả", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, kButtonWidth, kButtonHeight,
        m_hwnd, (HMENU)ID_RESULTS_EXPORT_ALL, g_hInstance, NULL
    );
    m_hExportWindowsButton = CreateWindowW(
        L"BUTTON", L"Xuất Windows", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, kButtonWidth, kButtonHeight,
        m_hwnd, (HMENU)ID_RESULTS_EXPORT_WINDOWS, g_hInstance, NULL
    );
    m_hExportOfficeButton = CreateWindowW(
        L"BUTTON", L"Xuất Office", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, kButtonWidth, kButtonHeight,
        m_hwnd, (HMENU)ID_RESULTS_EXPORT_OFFICE, g_hInstance, NULL
    );

    HFONT hFont = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (!hFont) {
        hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }
    SendMessage(m_hListView, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hCountLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hExportAllButton, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hExportWindowsButton, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hExportOfficeButton, WM_SETFONT, (WPARAM)hFont, TRUE);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    LayoutControls(rc.right, rc.bottom);
    return true;
}

void ResultsWindow::LayoutControls(int cx, int cy) {
    int listHeight = cy - kBottomRowHeight;
    if (listHeight < 50) listHeight = 50;
    if (m_hListView) {
        MoveWindow(m_hListView, kMargin, kMargin, cx - kMargin * 2, listHeight, TRUE);
    }

    int buttonY = cy - kMargin - kButtonHeight;
    int exportOfficeX = cx - kMargin - kButtonWidth;
    int exportWindowsX = exportOfficeX - kButtonSpacing - kButtonWidth;
    int exportAllX = exportWindowsX - kButtonSpacing - kButtonWidth;

    if (m_hExportOfficeButton) MoveWindow(m_hExportOfficeButton, exportOfficeX, buttonY, kButtonWidth, kButtonHeight, TRUE);
    if (m_hExportWindowsButton) MoveWindow(m_hExportWindowsButton, exportWindowsX, buttonY, kButtonWidth, kButtonHeight, TRUE);
    if (m_hExportAllButton) MoveWindow(m_hExportAllButton, exportAllX, buttonY, kButtonWidth, kButtonHeight, TRUE);

    if (m_hCountLabel) {
        int labelWidth = exportAllX - kMargin * 2;
        if (labelWidth < 100) labelWidth = 100;
        MoveWindow(m_hCountLabel, kMargin, buttonY + (kButtonHeight - 20) / 2, labelWidth, 20, TRUE);
    }
}

void ResultsWindow::PopulateList() {
    m_rows = LocalDatabase::ListHistory();

    ListView_DeleteAllItems(m_hListView);
    for (size_t i = 0; i < m_rows.size(); i++) {
        const ExcelReportRow& row = m_rows[i];
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = (int)i;
        item.iSubItem = ColTime;
        std::wstring reportedAt = Utf8Utils::Utf8ToWide(row.reportedAt);
        item.pszText = const_cast<wchar_t*>(reportedAt.c_str());
        int idx = ListView_InsertItem(m_hListView, &item);

        std::wstring hostname = Utf8Utils::Utf8ToWide(row.hostname);
        ListView_SetItemText(m_hListView, idx, ColHostname, const_cast<wchar_t*>(hostname.c_str()));
        std::wstring os = Utf8Utils::Utf8ToWide(row.osVersion);
        ListView_SetItemText(m_hListView, idx, ColOS, const_cast<wchar_t*>(os.c_str()));
        std::wstring win = Utf8Utils::Utf8ToWide(row.windowsStatusLabel);
        ListView_SetItemText(m_hListView, idx, ColWindows, const_cast<wchar_t*>(win.c_str()));
        std::wstring off = Utf8Utils::Utf8ToWide(row.officeStatusLabel);
        ListView_SetItemText(m_hListView, idx, ColOffice, const_cast<wchar_t*>(off.c_str()));
    }

    std::wstring countText = L"Tổng " + std::to_wstring(m_rows.size()) + L" kết quả đã lưu. Tick chọn dòng muốn xuất Excel.";
    SetWindowTextW(m_hCountLabel, countText.c_str());
}

std::vector<ExcelReportRow> ResultsWindow::GetCheckedRows() const {
    std::vector<ExcelReportRow> checked;
    int count = ListView_GetItemCount(m_hListView);
    for (int i = 0; i < count; i++) {
        if (ListView_GetCheckState(m_hListView, i) && i < (int)m_rows.size()) {
            checked.push_back(m_rows[i]);
        }
    }
    return checked;
}

void ResultsWindow::ExportSelected(ExcelReportMode mode) {
    std::vector<ExcelReportRow> checked = GetCheckedRows();
    if (checked.empty()) {
        MessageBoxW(m_hwnd, L"Vui lòng tick chọn ít nhất 1 dòng để xuất.", L"Xuất Excel", MB_OK | MB_ICONWARNING);
        return;
    }

    wchar_t fileBuf[MAX_PATH] = L"ket_qua_kiem_tra.xlsx";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"Excel Workbook (*.xlsx)\0*.xlsx\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"xlsx";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&ofn)) {
        return; // user cancelled
    }

    std::string filePath = Utf8Utils::WideToAnsiPath(fileBuf);
    std::string error = ExcelReport::SaveOrAppend(filePath, checked, mode);

    if (error.empty()) {
        std::wstring msg = L"Đã xuất " + std::to_wstring(checked.size()) + L" dòng vào:\n" + std::wstring(fileBuf);
        MessageBoxW(m_hwnd, msg.c_str(), L"Xuất Excel", MB_OK | MB_ICONINFORMATION);
    } else {
        std::wstring msg = L"Không xuất được file Excel:\n" + Utf8Utils::Utf8ToWide(error);
        MessageBoxW(m_hwnd, msg.c_str(), L"Xuất Excel", MB_OK | MB_ICONERROR);
    }
}

void ResultsWindow::OnCommand(WPARAM wParam, LPARAM lParam) {
    int wmId = LOWORD(wParam);
    if (wmId == ID_RESULTS_EXPORT_ALL) {
        ExportSelected(ExcelReportMode::All);
    } else if (wmId == ID_RESULTS_EXPORT_WINDOWS) {
        ExportSelected(ExcelReportMode::WindowsOnly);
    } else if (wmId == ID_RESULTS_EXPORT_OFFICE) {
        ExportSelected(ExcelReportMode::OfficeOnly);
    }
}

LRESULT CALLBACK ResultsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ResultsWindow* pThis = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<ResultsWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = reinterpret_cast<ResultsWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!pThis) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
    case WM_SIZE:
        pThis->LayoutControls(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_COMMAND:
        pThis->OnCommand(wParam, lParam);
        return 0;

    case WM_CLOSE:
        // Just hide/destroy this window - unlike MainWindow's WM_CLOSE, this
        // must NOT PostQuitMessage, or closing this secondary window would
        // shut down the whole app.
        DestroyWindow(hwnd);
        return 0;

    case WM_NCDESTROY:
        // Runs after the window is fully gone - safe to free `this` and
        // clear the singleton so the next "Xem kết quả kiểm tra" click
        // creates a fresh window instead of using a dangling pointer.
        delete pThis;
        if (s_instance == pThis) {
            s_instance = nullptr;
        }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}
