#pragma once

#include <windows.h>
#include <vector>
#include "../license-detection/ExcelReport.h"

// "Xem kết quả kiểm tra" - a secondary, non-modal window listing every
// check result saved so far (LocalDatabase::ListHistory()), with a
// checkbox per row and 3 export buttons that write the checked rows to an
// .xlsx file (create or append) via ExcelReport, in the same
// All/WindowsOnly/OfficeOnly modes the server's quick-export buttons use.
class ResultsWindow {
public:
    // Creates the window on first call; on later calls, just refreshes its
    // data (from the db, as of right now) and brings it to front. Never
    // more than one instance at a time.
    static void ShowOrActivate(HWND owner);

private:
    ResultsWindow();

    bool Create(HWND owner);
    void PopulateList();
    void LayoutControls(int cx, int cy);
    std::vector<ExcelReportRow> GetCheckedRows() const;
    void ExportSelected(ExcelReportMode mode);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnCommand(WPARAM wParam, LPARAM lParam);

    HWND m_hwnd;
    HWND m_hListView;
    HWND m_hExportAllButton;
    HWND m_hExportWindowsButton;
    HWND m_hExportOfficeButton;
    HWND m_hCountLabel;

    std::vector<ExcelReportRow> m_rows;

    static ResultsWindow* s_instance;
};
