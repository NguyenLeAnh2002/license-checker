#pragma once

#include <string>
#include <vector>
#include "LicenseResult.h"

// One exportable row - same column set as license_checker_server's Excel
// export (internal/export/report_export.go: AllColumns + the Windows/Office
// detail columns). session_name/site_name stay blank: this client has no
// session/site concept of its own.
struct ExcelReportRow {
    std::string orgUnit;
    std::string sessionName;
    std::string siteName;
    std::string hostname;
    std::string ip;
    std::string mac;
    std::string osVersion;
    std::string windowsStatusLabel; // Vietnamese label, e.g. "Hợp lệ"
    std::string officeStatusLabel;
    std::string discoveredAt;
    std::string reportedAt;

    std::string winName;
    std::string winProductName;
    std::string winDescription;
    std::string winPartialKey;
    std::string winKms;       // "Có"/"Không"
    std::string winKmsServer;

    std::string offProductName;
    std::string offPartialKey;
    std::string offKms;       // "Có"/"Không"
    std::string offKmsServer;
    std::string offTimeLeft;
    std::string offRenewInterval;
};

// Mode narrows a workbook to one license pillar, mirroring
// license_checker_server's export.Mode - used by "Xem kết quả kiểm tra"'s
// "Xuất Windows"/"Xuất Office" buttons to drop the other pillar's columns
// and summary numbers entirely, not just leave them blank.
enum class ExcelReportMode { All, WindowsOnly, OfficeOnly };

// Writes/appends a "KetQua"+"TongKet" report in the same two-sheet .xlsx
// format license_checker_server produces (same headers/order, same
// Đơn vị-grouped summary sheet) - so a file exported here can be merged with
// server-exported reports via the server's "Gộp kết quả Excel" feature.
class ExcelReport {
public:
    // Builds one row from a fresh check result. orgUnit is whatever context
    // is available on this machine (e.g. the Settings tab's Department
    // field, if the user ever sets it) - "" otherwise, never fabricated.
    static ExcelReportRow RowFromResult(const LicenseResult& result, const std::string& orgUnit);

    // If filePath already exists, reads its existing "KetQua" rows
    // (tolerant of a narrower/differently-ordered column set - e.g. a file
    // previously created with a different mode), appends newRows, recomputes
    // "TongKet", and rewrites both sheets - dropped to just the columns/
    // summary numbers `mode` calls for. If filePath doesn't exist, creates a
    // fresh workbook containing just newRows. Returns "" on success, or a
    // human-readable error message otherwise.
    static std::string SaveOrAppend(const std::string& filePath, const std::vector<ExcelReportRow>& newRows, ExcelReportMode mode);
};
