#include "ExcelReport.h"
#include "LicenseResultMapping.h"
#include "../third_party/miniz/miniz.h"

#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

// A minimal hand-rolled OOXML (.xlsx) reader/writer, scoped to exactly what
// this feature needs: a 2-sheet ("KetQua"/"TongKet") workbook with plain
// text cells, matching license_checker_server's own export format
// (internal/export/report_export.go) column-for-column - including its
// exact Vietnamese header titles, so a file produced here reads identically
// to (and can even be merged with, via the server's "Gộp kết quả Excel")
// one produced by that server. Not a general xlsx library: the reader
// assumes every data row has exactly as many cells as the header (true for
// both this writer's own output and excelize's, which never skips empty
// cells), and the writer only ever emits inline-string cells.

namespace {

using RowMap = std::map<std::string, std::string>;

// key -> Vietnamese title, in export column order - identical to Go's
// export.AllColumns with the Windows/Office detail columns inlined right
// after their coarse status column, matching the "export everything"
// default (Mode ModeAll, no column picker) since this client has no column
// picker of its own.
const std::vector<std::pair<std::string, std::string>>& Columns() {
    static const std::vector<std::pair<std::string, std::string>> cols = {
        {"org_unit", "Đơn vị"},
        {"session_name", "Phiên kiểm tra"},
        {"site_name", "Site"},
        {"hostname", "Tên máy"},
        {"ip", "IP"},
        {"mac", "MAC"},
        {"os_version", "Hệ điều hành"},
        {"windows_status", "Windows License"},
        {"win_name", "Windows - Tên"},
        {"win_product_name", "Windows - Phiên bản"},
        {"win_description", "Windows - Mô tả"},
        {"win_partial_key", "Windows - Khóa"},
        {"win_kms", "Windows - Dùng KMS"},
        {"win_kms_server", "Windows - Máy chủ KMS"},
        {"office_status", "Office License"},
        {"off_product_name", "Office - Phiên bản"},
        {"off_partial_key", "Office - Khóa"},
        {"off_kms", "Office - Dùng KMS"},
        {"off_kms_server", "Office - Máy chủ KMS"},
        {"off_time_left", "Office - Thời hạn còn lại (ngày)"},
        {"off_renew_interval", "Office - Chu kỳ gia hạn (phút)"},
        {"discovered_at", "Thời điểm quét"},
        {"reported_at", "Thời điểm báo cáo"},
    };
    return cols;
}

// Drops the out-of-scope pillar's column (and its win_*/off_* detail
// columns) entirely, regardless of mode - mirrors the server's
// resolveColumns(mode) in report_export.go.
std::vector<std::pair<std::string, std::string>> ColumnsForMode(ExcelReportMode mode) {
    std::vector<std::pair<std::string, std::string>> out;
    for (auto& c : Columns()) {
        if (mode == ExcelReportMode::WindowsOnly && (c.first == "office_status" || c.first.rfind("off_", 0) == 0)) {
            continue;
        }
        if (mode == ExcelReportMode::OfficeOnly && (c.first == "windows_status" || c.first.rfind("win_", 0) == 0)) {
            continue;
        }
        out.push_back(c);
    }
    return out;
}

const std::vector<std::pair<std::string, std::string>>& SummaryColumns() {
    static const std::vector<std::pair<std::string, std::string>> cols = {
        {"org_unit", "Đơn vị"},
        {"total", "Số máy trong vùng kiểm tra"},
        {"checked", "Số máy đã kiểm tra được"},
        {"windows_valid", "Số máy Windows có bản quyền"},
        {"office_valid", "Số máy Office có bản quyền"},
        {"windows_pct", "Tỉ lệ máy có Windows bản quyền (%)"},
        {"office_pct", "Tỉ lệ máy có Office bản quyền (%)"},
    };
    return cols;
}

std::vector<std::pair<std::string, std::string>> SummaryColumnsForMode(ExcelReportMode mode) {
    std::vector<std::pair<std::string, std::string>> out;
    for (auto& c : SummaryColumns()) {
        if (mode == ExcelReportMode::WindowsOnly && (c.first == "office_valid" || c.first == "office_pct")) {
            continue;
        }
        if (mode == ExcelReportMode::OfficeOnly && (c.first == "windows_valid" || c.first == "windows_pct")) {
            continue;
        }
        out.push_back(c);
    }
    return out;
}

RowMap RowMapFromRow(const ExcelReportRow& row) {
    RowMap m;
    m["org_unit"] = row.orgUnit;
    m["session_name"] = row.sessionName;
    m["site_name"] = row.siteName;
    m["hostname"] = row.hostname;
    m["ip"] = row.ip;
    m["mac"] = row.mac;
    m["os_version"] = row.osVersion;
    m["windows_status"] = row.windowsStatusLabel;
    m["win_name"] = row.winName;
    m["win_product_name"] = row.winProductName;
    m["win_description"] = row.winDescription;
    m["win_partial_key"] = row.winPartialKey;
    m["win_kms"] = row.winKms;
    m["win_kms_server"] = row.winKmsServer;
    m["office_status"] = row.officeStatusLabel;
    m["off_product_name"] = row.offProductName;
    m["off_partial_key"] = row.offPartialKey;
    m["off_kms"] = row.offKms;
    m["off_kms_server"] = row.offKmsServer;
    m["off_time_left"] = row.offTimeLeft;
    m["off_renew_interval"] = row.offRenewInterval;
    m["discovered_at"] = row.discoveredAt;
    m["reported_at"] = row.reportedAt;
    return m;
}

std::string ColumnLetter(int index0) {
    std::string s;
    int n = index0 + 1;
    while (n > 0) {
        int rem = (n - 1) % 26;
        s = char('A' + rem) + s;
        n = (n - 1) / 26;
    }
    return s;
}

std::string XmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            default:   out += c;
        }
    }
    return out;
}

std::string XmlUnescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '&') {
            if (s.compare(i, 5, "&amp;") == 0) { out += '&'; i += 5; continue; }
            if (s.compare(i, 4, "&lt;") == 0) { out += '<'; i += 4; continue; }
            if (s.compare(i, 4, "&gt;") == 0) { out += '>'; i += 4; continue; }
            if (s.compare(i, 6, "&quot;") == 0) { out += '"'; i += 6; continue; }
            if (s.compare(i, 6, "&apos;") == 0) { out += '\''; i += 6; continue; }
        }
        out += s[i];
        i++;
    }
    return out;
}

std::string PercentString(int count, int total) {
    if (total == 0) {
        return "-";
    }
    double pct = static_cast<double>(count) / static_cast<double>(total) * 100.0;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", pct);
    return buf;
}

// ---- Minimal XML element scanning (reading side) ----
// Finds the next top-level <tagName ...>...</tagName> (or self-closing
// <tagName .../>) at or after pos, assuming no same-name nesting - true for
// every element this reader looks at (row/c/si/t/sheet/Relationship).
bool NextElement(const std::string& xml, size_t& pos, const std::string& tagName, std::string& outOpenTag, std::string& outInner) {
    std::string openPattern = "<" + tagName;
    while (true) {
        size_t start = xml.find(openPattern, pos);
        if (start == std::string::npos) {
            return false;
        }
        char after = (start + openPattern.size() < xml.size()) ? xml[start + openPattern.size()] : '\0';
        if (after != ' ' && after != '>' && after != '/') {
            pos = start + openPattern.size();
            continue;
        }
        size_t gt = xml.find('>', start);
        if (gt == std::string::npos) {
            return false;
        }
        bool selfClosing = xml[gt - 1] == '/';
        outOpenTag = xml.substr(start, gt - start + 1);
        if (selfClosing) {
            outInner.clear();
            pos = gt + 1;
            return true;
        }
        std::string closeTag = "</" + tagName + ">";
        size_t closeStart = xml.find(closeTag, gt + 1);
        if (closeStart == std::string::npos) {
            return false;
        }
        outInner = xml.substr(gt + 1, closeStart - gt - 1);
        pos = closeStart + closeTag.size();
        return true;
    }
}

std::vector<std::pair<std::string, std::string>> AllElements(const std::string& xml, const std::string& tagName) {
    std::vector<std::pair<std::string, std::string>> out;
    size_t pos = 0;
    std::string openTag, inner;
    while (NextElement(xml, pos, tagName, openTag, inner)) {
        out.push_back({openTag, inner});
    }
    return out;
}

std::string GetAttr(const std::string& openTag, const std::string& attrName) {
    std::string pattern = attrName + "=\"";
    size_t p = openTag.find(pattern);
    if (p == std::string::npos) {
        return "";
    }
    p += pattern.size();
    size_t end = openTag.find('"', p);
    if (end == std::string::npos) {
        return "";
    }
    return openTag.substr(p, end - p);
}

std::vector<std::string> ParseSharedStrings(const std::string& xml) {
    std::vector<std::string> out;
    for (auto& si : AllElements(xml, "si")) {
        std::string text;
        for (auto& t : AllElements(si.second, "t")) {
            text += XmlUnescape(t.second);
        }
        out.push_back(text);
    }
    return out;
}

std::vector<std::vector<std::string>> ParseSheetRows(const std::string& xml, const std::vector<std::string>& sharedStrings) {
    std::string sheetData = xml;
    {
        size_t p = 0;
        std::string openTag, inner;
        if (NextElement(xml, p, "sheetData", openTag, inner)) {
            sheetData = inner;
        }
    }

    std::vector<std::vector<std::string>> rows;
    for (auto& row : AllElements(sheetData, "row")) {
        std::vector<std::string> cells;
        for (auto& cell : AllElements(row.second, "c")) {
            std::string t = GetAttr(cell.first, "t");
            std::string value;
            if (t == "s") {
                size_t p = 0;
                std::string vOpen, vInner;
                if (NextElement(cell.second, p, "v", vOpen, vInner)) {
                    int idx = atoi(vInner.c_str());
                    if (idx >= 0 && idx < static_cast<int>(sharedStrings.size())) {
                        value = sharedStrings[idx];
                    }
                }
            } else if (t == "inlineStr") {
                size_t p = 0;
                std::string isOpen, isInner;
                if (NextElement(cell.second, p, "is", isOpen, isInner)) {
                    for (auto& t2 : AllElements(isInner, "t")) {
                        value += XmlUnescape(t2.second);
                    }
                }
            } else {
                size_t p = 0;
                std::string vOpen, vInner;
                if (NextElement(cell.second, p, "v", vOpen, vInner)) {
                    value = XmlUnescape(vInner);
                }
            }
            cells.push_back(value);
        }
        rows.push_back(cells);
    }
    return rows;
}

bool ResolveSheetTarget(const std::string& workbookXml, const std::string& relsXml, const std::string& sheetName, std::string& outTarget) {
    std::string rid;
    for (auto& sheet : AllElements(workbookXml, "sheet")) {
        if (GetAttr(sheet.first, "name") == sheetName) {
            rid = GetAttr(sheet.first, "r:id");
            break;
        }
    }
    if (rid.empty()) {
        return false;
    }
    for (auto& rel : AllElements(relsXml, "Relationship")) {
        if (GetAttr(rel.first, "Id") == rid) {
            outTarget = GetAttr(rel.first, "Target");
            return !outTarget.empty();
        }
    }
    return false;
}

bool ExtractZipPart(mz_zip_archive& zip, const char* name, std::string& out) {
    size_t size = 0;
    void* buf = mz_zip_reader_extract_file_to_heap(&zip, name, &size, 0);
    if (!buf) {
        return false;
    }
    out.assign(reinterpret_cast<char*>(buf), size);
    mz_free(buf);
    return true;
}

// Reads filePath's existing "KetQua" sheet back into RowMaps (matched by
// header title, tolerant of a narrower or differently-ordered column set,
// same as the server's own xlsximport.ParseWorkbook). Returns false (with a
// message in outError) only on an actual read/format failure - the caller
// must not proceed to overwrite the file in that case.
bool ReadExistingKetQua(const std::string& filePath, std::vector<RowMap>& outRows, std::string& outError) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, filePath.c_str(), 0)) {
        outError = "không mở được (không đúng định dạng .xlsx?)";
        return false;
    }

    std::string workbookXml, relsXml;
    if (!ExtractZipPart(zip, "xl/workbook.xml", workbookXml) || !ExtractZipPart(zip, "xl/_rels/workbook.xml.rels", relsXml)) {
        mz_zip_reader_end(&zip);
        outError = "thiếu workbook.xml/rels";
        return false;
    }

    std::string target;
    if (!ResolveSheetTarget(workbookXml, relsXml, "KetQua", target)) {
        mz_zip_reader_end(&zip);
        outError = "không tìm thấy sheet KetQua";
        return false;
    }

    std::string sheetXml;
    if (!ExtractZipPart(zip, ("xl/" + target).c_str(), sheetXml)) {
        mz_zip_reader_end(&zip);
        outError = "không đọc được sheet KetQua";
        return false;
    }

    std::string sharedXml;
    ExtractZipPart(zip, "xl/sharedStrings.xml", sharedXml); // optional
    mz_zip_reader_end(&zip);

    std::vector<std::string> sharedStrings;
    if (!sharedXml.empty()) {
        sharedStrings = ParseSharedStrings(sharedXml);
    }

    auto rows = ParseSheetRows(sheetXml, sharedStrings);
    if (rows.empty()) {
        return true; // no header row at all - nothing to carry over
    }

    std::map<std::string, std::string> titleToKey;
    for (auto& col : Columns()) {
        titleToKey[col.second] = col.first;
    }

    const std::vector<std::string>& header = rows[0];
    for (size_t r = 1; r < rows.size(); r++) {
        const std::vector<std::string>& cells = rows[r];
        RowMap m;
        for (size_t i = 0; i < header.size(); i++) {
            auto it = titleToKey.find(header[i]);
            if (it == titleToKey.end()) {
                continue;
            }
            m[it->second] = (i < cells.size()) ? cells[i] : "";
        }
        outRows.push_back(m);
    }
    return true;
}

// ---- Writing side ----

std::string BuildSheetXml(const std::vector<std::string>& headers, const std::vector<std::string>& keys, const std::vector<RowMap>& rows) {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
    xml << "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>";

    xml << "<row r=\"1\">";
    for (size_t i = 0; i < headers.size(); i++) {
        xml << "<c r=\"" << ColumnLetter(static_cast<int>(i)) << "1\" t=\"inlineStr\" s=\"1\"><is><t>" << XmlEscape(headers[i]) << "</t></is></c>";
    }
    xml << "</row>";

    for (size_t r = 0; r < rows.size(); r++) {
        xml << "<row r=\"" << (r + 2) << "\">";
        for (size_t i = 0; i < keys.size(); i++) {
            auto it = rows[r].find(keys[i]);
            std::string val = (it != rows[r].end()) ? it->second : "";
            xml << "<c r=\"" << ColumnLetter(static_cast<int>(i)) << (r + 2) << "\" t=\"inlineStr\"><is><t>" << XmlEscape(val) << "</t></is></c>";
        }
        xml << "</row>";
    }

    xml << "</sheetData></worksheet>";
    return xml.str();
}

std::vector<RowMap> SummarizeByOrgUnit(const std::vector<RowMap>& rows) {
    struct Agg {
        int total = 0, checked = 0, winValid = 0, offValid = 0;
    };
    std::map<std::string, Agg> byUnit;
    std::vector<std::string> order;
    std::string validLabel = StatusLabelVi("valid");

    for (auto& r : rows) {
        std::string unit;
        auto itUnit = r.find("org_unit");
        if (itUnit != r.end()) unit = itUnit->second;

        if (byUnit.find(unit) == byUnit.end()) {
            byUnit[unit] = Agg{};
            order.push_back(unit);
        }
        Agg& a = byUnit[unit];
        a.total++;

        auto itReported = r.find("reported_at");
        if (itReported != r.end() && !itReported->second.empty()) {
            a.checked++;
        }
        auto itWin = r.find("windows_status");
        if (itWin != r.end() && itWin->second == validLabel) {
            a.winValid++;
        }
        auto itOff = r.find("office_status");
        if (itOff != r.end() && itOff->second == validLabel) {
            a.offValid++;
        }
    }

    std::sort(order.begin(), order.end());
    std::vector<RowMap> out;
    for (auto& unit : order) {
        Agg& a = byUnit[unit];
        RowMap m;
        m["org_unit"] = unit;
        m["total"] = std::to_string(a.total);
        m["checked"] = std::to_string(a.checked);
        m["windows_valid"] = std::to_string(a.winValid);
        m["office_valid"] = std::to_string(a.offValid);
        m["windows_pct"] = PercentString(a.winValid, a.checked);
        m["office_pct"] = PercentString(a.offValid, a.checked);
        out.push_back(m);
    }
    return out;
}

std::string ContentTypesXml() {
    return
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet2.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
        "</Types>";
}

std::string RootRelsXml() {
    return
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "</Relationships>";
}

std::string WorkbookXml() {
    return
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets>"
        "<sheet name=\"KetQua\" sheetId=\"1\" r:id=\"rId1\"/>"
        "<sheet name=\"TongKet\" sheetId=\"2\" r:id=\"rId2\"/>"
        "</sheets>"
        "</workbook>";
}

std::string WorkbookRelsXml() {
    return
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet2.xml\"/>"
        "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "</Relationships>";
}

// Two cellXfs: 0 = default, 1 = header style (bold white text on the same
// dark-blue #1F4E78 fill the server's own export uses).
std::string StylesXml() {
    return
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<fonts count=\"2\">"
        "<font><sz val=\"11\"/><name val=\"Calibri\"/></font>"
        "<font><b/><sz val=\"11\"/><color rgb=\"FFFFFFFF\"/><name val=\"Calibri\"/></font>"
        "</fonts>"
        "<fills count=\"3\">"
        "<fill><patternFill patternType=\"none\"/></fill>"
        "<fill><patternFill patternType=\"gray125\"/></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FF1F4E78\"/><bgColor indexed=\"64\"/></patternFill></fill>"
        "</fills>"
        "<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"2\">"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
        "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"2\" borderId=\"0\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\"/>"
        "</cellXfs>"
        "</styleSheet>";
}

bool WriteZipFile(const std::string& filePath, const std::vector<std::pair<std::string, std::string>>& parts, std::string& outError) {
    std::string tmpPath = filePath + ".tmp";
    DeleteFileA(tmpPath.c_str()); // in case a previous failed attempt left one behind

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, tmpPath.c_str(), 0)) {
        outError = "Không tạo được file tạm để ghi Excel";
        return false;
    }

    bool ok = true;
    for (auto& part : parts) {
        if (!mz_zip_writer_add_mem(&zip, part.first.c_str(), part.second.data(), part.second.size(), MZ_BEST_COMPRESSION)) {
            ok = false;
            break;
        }
    }
    if (ok) {
        ok = mz_zip_writer_finalize_archive(&zip) != 0;
    }
    mz_zip_writer_end(&zip);

    if (!ok) {
        outError = "Lỗi ghi dữ liệu file Excel";
        DeleteFileA(tmpPath.c_str());
        return false;
    }
    if (!MoveFileExA(tmpPath.c_str(), filePath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        outError = "Không ghi đè được file đích (có đang mở trong Excel không?)";
        DeleteFileA(tmpPath.c_str());
        return false;
    }
    return true;
}

} // namespace

ExcelReportRow ExcelReport::RowFromResult(const LicenseResult& result, const std::string& orgUnit) {
    ExcelReportRow row;
    row.orgUnit = orgUnit;
    row.hostname = GetLocalHostname();
    row.osVersion = FormatOsVersion(result.GetWindowsVersion(), result.GetWindowsEdition());

    std::string winCode = NormalizeStatusCode(LicenseStatusToRealString(result.GetLicenseStatus()));
    std::string offCode = NormalizeStatusCode(OfficeStatusToRealString(result.GetOfficeLicenseInfo().licenseStatus));
    row.windowsStatusLabel = StatusLabelVi(winCode);
    row.officeStatusLabel = StatusLabelVi(offCode);

    const WindowsLicenseInfo& winInfo = result.GetWindowsLicenseInfo();
    row.winName = winInfo.name;
    row.winProductName = result.GetWindowsEdition();
    row.winDescription = winInfo.description;
    row.winPartialKey = winInfo.partialProductKey;
    row.winKms = result.GetWindowsKmsServer().empty() ? "Không" : "Có";
    row.winKmsServer = result.GetWindowsKmsServer();

    const OfficeLicenseInfo& offInfo = result.GetOfficeLicenseInfo();
    row.offProductName = offInfo.licenseName;
    row.offPartialKey = offInfo.partialProductKey;
    row.offKms = offInfo.kmsServer.empty() ? "Không" : "Có";
    row.offKmsServer = offInfo.kmsServer;

    int timeLeft = 0;
    if (ParseLeadingInt(offInfo.remainingGrace, timeLeft)) {
        row.offTimeLeft = std::to_string(timeLeft);
    }
    int renewInterval = 0;
    if (ParseLeadingInt(offInfo.renewalInterval, renewInterval)) {
        row.offRenewInterval = std::to_string(renewInterval);
    }

    std::string now = CurrentTimestampUtc();
    row.discoveredAt = now;
    row.reportedAt = now;
    return row;
}

std::string ExcelReport::SaveOrAppend(const std::string& filePath, const std::vector<ExcelReportRow>& newRows, ExcelReportMode mode) {
    std::vector<RowMap> rows;

    DWORD attrs = GetFileAttributesA(filePath.c_str());
    bool exists = attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
    if (exists) {
        std::string readError;
        if (!ReadExistingKetQua(filePath, rows, readError)) {
            return "Không đọc được file Excel hiện có (" + readError + ") - chưa ghi gì để tránh mất dữ liệu cũ.";
        }
    }
    for (auto& row : newRows) {
        rows.push_back(RowMapFromRow(row));
    }

    std::vector<std::string> ketQuaHeaders, ketQuaKeys;
    for (auto& col : ColumnsForMode(mode)) {
        ketQuaKeys.push_back(col.first);
        ketQuaHeaders.push_back(col.second);
    }
    std::string ketQuaXml = BuildSheetXml(ketQuaHeaders, ketQuaKeys, rows);

    std::vector<RowMap> summary = SummarizeByOrgUnit(rows);
    std::vector<std::string> summaryHeaders, summaryKeys;
    for (auto& col : SummaryColumnsForMode(mode)) {
        summaryKeys.push_back(col.first);
        summaryHeaders.push_back(col.second);
    }
    std::string tongKetXml = BuildSheetXml(summaryHeaders, summaryKeys, summary);

    std::vector<std::pair<std::string, std::string>> parts = {
        {"[Content_Types].xml", ContentTypesXml()},
        {"_rels/.rels", RootRelsXml()},
        {"xl/workbook.xml", WorkbookXml()},
        {"xl/_rels/workbook.xml.rels", WorkbookRelsXml()},
        {"xl/styles.xml", StylesXml()},
        {"xl/worksheets/sheet1.xml", ketQuaXml},
        {"xl/worksheets/sheet2.xml", tongKetXml},
    };

    std::string writeError;
    if (!WriteZipFile(filePath, parts, writeError)) {
        return writeError;
    }
    return "";
}
