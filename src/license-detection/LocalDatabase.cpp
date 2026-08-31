#include "LocalDatabase.h"
#include "LicenseResultMapping.h"
#include "FlatJson.h"
#include "../third_party/sqlite/sqlite3.h"

#include <windows.h>
#include <objbase.h>
#include <cstdio>
#include <ctime>

#pragma comment(lib, "ole32.lib")

namespace {

// A plain 32-hex-char unique id - the schema only requires TEXT PRIMARY KEY
// uniqueness, so this doesn't need to look like the server's own ULIDs.
std::string NewId() {
    GUID guid;
    if (FAILED(CoCreateGuid(&guid))) {
        // Extremely unlikely; fall back to a timestamp-based id rather than
        // failing the whole save over an id collision risk.
        char buf[32];
        snprintf(buf, sizeof(buf), "fallback%lld", (long long)time(nullptr));
        return std::string(buf);
    }
    char buf[33];
    snprintf(buf, sizeof(buf), "%08lX%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return std::string(buf);
}

const char kSchemaSql[] =
    "CREATE TABLE IF NOT EXISTS sites ("
    "  id TEXT PRIMARY KEY,"
    "  name TEXT NOT NULL,"
    "  created_at TEXT NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS sessions ("
    "  id TEXT PRIMARY KEY,"
    "  site_id TEXT NOT NULL REFERENCES sites(id),"
    "  name TEXT NOT NULL,"
    "  org_unit TEXT NOT NULL,"
    "  started_at TEXT NOT NULL,"
    "  ended_at TEXT,"
    "  status TEXT NOT NULL DEFAULT 'active',"
    "  note TEXT NOT NULL DEFAULT '',"
    "  is_orphan INTEGER NOT NULL DEFAULT 0,"
    "  updated_at TEXT NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS scanned_machines ("
    "  id TEXT PRIMARY KEY,"
    "  session_id TEXT NOT NULL REFERENCES sessions(id),"
    "  hostname TEXT NOT NULL DEFAULT '',"
    "  ip TEXT NOT NULL,"
    "  mac TEXT NOT NULL DEFAULT '',"
    "  discovered_at TEXT NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS license_reports ("
    "  id TEXT PRIMARY KEY,"
    "  session_id TEXT NOT NULL REFERENCES sessions(id),"
    "  hostname TEXT NOT NULL DEFAULT '',"
    "  ip TEXT NOT NULL,"
    "  os_version TEXT NOT NULL DEFAULT '',"
    "  windows_license_json TEXT NOT NULL DEFAULT '',"
    "  office_license_json TEXT NOT NULL DEFAULT '',"
    "  windows_status TEXT NOT NULL DEFAULT 'unknown',"
    "  office_status TEXT NOT NULL DEFAULT 'unknown',"
    "  raw_json TEXT NOT NULL DEFAULT '',"
    "  reported_at TEXT NOT NULL"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_sessions_site ON sessions(site_id);"
    "CREATE INDEX IF NOT EXISTS idx_scanned_machines_session ON scanned_machines(session_id);"
    "CREATE INDEX IF NOT EXISTS idx_reports_session ON license_reports(session_id);";

bool ExecSimple(sqlite3* db, const char* sql, std::string& outError) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        outError = errMsg ? errMsg : sqlite3_errmsg(db);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// Binds `param` as the sole "WHERE ... = ?" text param and returns the
// first column of the first row, or "" if there's no match.
std::string QueryScalarText(sqlite3* db, const char* sql, const std::string& param) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return "";
    }
    sqlite3_bind_text(stmt, 1, param.c_str(), -1, SQLITE_TRANSIENT);
    std::string result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text) {
            result = reinterpret_cast<const char*>(text);
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

// Finds (by hostname) or creates the one "site" this machine's local db
// keeps all its saves under.
std::string FindOrCreateSite(sqlite3* db, const std::string& hostname, const std::string& now, std::string& outError) {
    std::string existing = QueryScalarText(db, "SELECT id FROM sites WHERE name = ? LIMIT 1", hostname);
    if (!existing.empty()) {
        return existing;
    }

    std::string id = NewId();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "INSERT INTO sites (id, name, created_at) VALUES (?, ?, ?)", -1, &stmt, nullptr) != SQLITE_OK) {
        outError = sqlite3_errmsg(db);
        return "";
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hostname.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        outError = sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
    return ok ? id : "";
}

// Finds (by site_id + is_orphan=1) or creates the one "unassigned reports"
// session every local save appends into - mirrors
// license_checker_server's GetOrCreateOrphanSession.
std::string FindOrCreateOrphanSession(sqlite3* db, const std::string& siteId, const std::string& now, std::string& outError) {
    std::string existing = QueryScalarText(db, "SELECT id FROM sessions WHERE site_id = ? AND is_orphan = 1 LIMIT 1", siteId);
    if (!existing.empty()) {
        sqlite3_stmt* touch = nullptr;
        if (sqlite3_prepare_v2(db, "UPDATE sessions SET updated_at = ? WHERE id = ?", -1, &touch, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(touch, 1, now.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(touch, 2, existing.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(touch);
            sqlite3_finalize(touch);
        }
        return existing;
    }

    std::string id = NewId();
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO sessions (id, site_id, name, org_unit, started_at, status, note, is_orphan, updated_at) "
        "VALUES (?, ?, 'Kiểm tra cục bộ', '', ?, 'active', '', 1, ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        outError = sqlite3_errmsg(db);
        return "";
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, siteId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        outError = sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
    return ok ? id : "";
}

bool InsertScannedMachine(sqlite3* db, const std::string& sessionId, const std::string& hostname, const std::string& now, std::string& outError) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO scanned_machines (id, session_id, hostname, ip, mac, discovered_at) VALUES (?, ?, ?, '', '', ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        outError = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(stmt, 1, NewId().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, sessionId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, hostname.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        outError = sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
    return ok;
}

// Builds the same windows_license/office_license JSON object shapes
// ServerReporter::BuildPayload sends over HTTP, so a report saved locally
// and one reported live look identical once imported into the server.
std::string BuildWindowsLicenseJson(const LicenseResult& result, const std::string& checkTime) {
    const auto& info = result.GetWindowsLicenseInfo();
    bool hasKmsServer = !result.GetWindowsKmsServer().empty();
    std::string json = "{";
    json += "\"name\":\"" + JsonEscape(info.name) + "\",";
    json += "\"product_name\":\"" + JsonEscape(result.GetWindowsEdition()) + "\",";
    json += "\"description\":\"" + JsonEscape(info.description) + "\",";
    json += "\"license_status\":\"" + LicenseStatusToRealString(result.GetLicenseStatus()) + "\",";
    json += "\"partial_key\":\"" + JsonEscape(info.partialProductKey) + "\",";
    json += std::string("\"kms\":") + (hasKmsServer ? "true" : "false") + ",";
    json += "\"kms_server\":\"" + JsonEscape(result.GetWindowsKmsServer()) + "\",";
    json += "\"check_time\":\"" + checkTime + "\"";
    json += "}";
    return json;
}

std::string BuildOfficeLicenseJson(const LicenseResult& result, const std::string& checkTime) {
    const auto& info = result.GetOfficeLicenseInfo();
    std::string json = "{";
    json += "\"license_status\":\"" + OfficeStatusToRealString(info.licenseStatus) + "\",";
    json += "\"product_name\":\"" + JsonEscape(info.licenseName) + "\",";
    json += "\"partial_key\":\"" + JsonEscape(info.partialProductKey) + "\",";
    json += "\"kms_server\":\"" + JsonEscape(info.kmsServer) + "\",";

    int timeLeft = 0;
    ParseLeadingInt(info.remainingGrace, timeLeft);
    json += "\"time_left\":" + std::to_string(timeLeft) + ",";

    int renewInterval = 0;
    if (ParseLeadingInt(info.renewalInterval, renewInterval)) {
        json += "\"renew_interval\":" + std::to_string(renewInterval) + ",";
    }
    json += "\"check_time\":\"" + checkTime + "\"";
    json += "}";
    return json;
}

bool InsertLicenseReport(sqlite3* db, const std::string& sessionId, const std::string& hostname,
                          const std::string& osVersion, const std::string& windowsJson, const std::string& officeJson,
                          const std::string& windowsStatus, const std::string& officeStatus,
                          const std::string& now, std::string& outError) {
    std::string rawJson = "{\"hostname\":\"" + JsonEscape(hostname) + "\",\"ip\":\"\",\"os_version\":\"" + JsonEscape(osVersion) +
        "\",\"windows_license\":" + windowsJson + ",\"office_license\":" + officeJson + "}";

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO license_reports "
        "(id, session_id, hostname, ip, os_version, windows_license_json, office_license_json, windows_status, office_status, raw_json, reported_at) "
        "VALUES (?, ?, ?, '', ?, ?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        outError = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(stmt, 1, NewId().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, sessionId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, hostname.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, osVersion.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, windowsJson.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, officeJson.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, windowsStatus.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, officeStatus.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, rawJson.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, now.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        outError = sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
    return ok;
}

// Comparable view of a report's substantive fields, deliberately excluding
// check_time (which always differs between two otherwise-identical checks,
// so comparing raw JSON text would never dedupe anything). Two reports
// with an equal snapshot are "the same result" for the purposes of the
// save button's "check trùng" behavior.
struct ReportSnapshot {
    std::string windowsStatus, officeStatus, osVersion;
    std::string winName, winProductName, winDescription, winPartialKey, winKmsServer;
    bool winKms = false;
    std::string offProductName, offPartialKey, offKmsServer;
    int offTimeLeft = 0;
    bool hasOffRenewInterval = false;
    int offRenewInterval = 0;

    bool operator==(const ReportSnapshot& o) const {
        return windowsStatus == o.windowsStatus && officeStatus == o.officeStatus && osVersion == o.osVersion &&
               winName == o.winName && winProductName == o.winProductName && winDescription == o.winDescription &&
               winPartialKey == o.winPartialKey && winKmsServer == o.winKmsServer && winKms == o.winKms &&
               offProductName == o.offProductName && offPartialKey == o.offPartialKey && offKmsServer == o.offKmsServer &&
               offTimeLeft == o.offTimeLeft && hasOffRenewInterval == o.hasOffRenewInterval &&
               (!hasOffRenewInterval || offRenewInterval == o.offRenewInterval);
    }
};

ReportSnapshot SnapshotFromFields(const std::string& windowsStatus, const std::string& officeStatus, const std::string& osVersion,
                                  const std::string& windowsJson, const std::string& officeJson) {
    ReportSnapshot snap;
    snap.windowsStatus = windowsStatus;
    snap.officeStatus = officeStatus;
    snap.osVersion = osVersion;
    FlatJson::GetString(windowsJson, "name", snap.winName);
    FlatJson::GetString(windowsJson, "product_name", snap.winProductName);
    FlatJson::GetString(windowsJson, "description", snap.winDescription);
    FlatJson::GetString(windowsJson, "partial_key", snap.winPartialKey);
    FlatJson::GetString(windowsJson, "kms_server", snap.winKmsServer);
    FlatJson::GetBool(windowsJson, "kms", snap.winKms);
    FlatJson::GetString(officeJson, "product_name", snap.offProductName);
    FlatJson::GetString(officeJson, "partial_key", snap.offPartialKey);
    FlatJson::GetString(officeJson, "kms_server", snap.offKmsServer);
    FlatJson::GetInt(officeJson, "time_left", snap.offTimeLeft);
    snap.hasOffRenewInterval = FlatJson::GetInt(officeJson, "renew_interval", snap.offRenewInterval);
    return snap;
}

struct LastReportRow {
    bool found = false;
    std::string windowsStatus, officeStatus, osVersion, windowsJson, officeJson;
};

// The most recently saved report for this session (there's only ever one
// orphan session per local db, so this is effectively "the last thing
// saved on this machine").
LastReportRow QueryLastReport(sqlite3* db, const std::string& sessionId) {
    LastReportRow row;
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT windows_status, office_status, os_version, windows_license_json, office_license_json "
        "FROM license_reports WHERE session_id = ? ORDER BY reported_at DESC LIMIT 1";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return row;
    }
    sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto col = [&](int i) {
            const unsigned char* t = sqlite3_column_text(stmt, i);
            return t ? std::string(reinterpret_cast<const char*>(t)) : std::string();
        };
        row.found = true;
        row.windowsStatus = col(0);
        row.officeStatus = col(1);
        row.osVersion = col(2);
        row.windowsJson = col(3);
        row.officeJson = col(4);
    }
    sqlite3_finalize(stmt);
    return row;
}

// Reconstructs an ExcelReportRow (the shape both "Xem kết quả kiểm tra" and
// the Excel export already use) from a stored license_reports row, parsing
// its two JSON blobs back into fields via FlatJson.
ExcelReportRow RowFromStoredReport(const std::string& hostname, const std::string& osVersion,
                                    const std::string& windowsStatus, const std::string& officeStatus,
                                    const std::string& windowsJson, const std::string& officeJson,
                                    const std::string& reportedAt) {
    ExcelReportRow row;
    row.hostname = hostname;
    row.osVersion = osVersion;
    row.windowsStatusLabel = StatusLabelVi(windowsStatus);
    row.officeStatusLabel = StatusLabelVi(officeStatus);
    row.discoveredAt = reportedAt;
    row.reportedAt = reportedAt;

    FlatJson::GetString(windowsJson, "name", row.winName);
    FlatJson::GetString(windowsJson, "product_name", row.winProductName);
    FlatJson::GetString(windowsJson, "description", row.winDescription);
    FlatJson::GetString(windowsJson, "partial_key", row.winPartialKey);
    bool winKms = false;
    FlatJson::GetBool(windowsJson, "kms", winKms);
    row.winKms = winKms ? "Có" : "Không";
    FlatJson::GetString(windowsJson, "kms_server", row.winKmsServer);

    FlatJson::GetString(officeJson, "product_name", row.offProductName);
    FlatJson::GetString(officeJson, "partial_key", row.offPartialKey);
    FlatJson::GetString(officeJson, "kms_server", row.offKmsServer);
    row.offKms = row.offKmsServer.empty() ? "Không" : "Có";
    int timeLeft = 0;
    if (FlatJson::GetInt(officeJson, "time_left", timeLeft)) {
        row.offTimeLeft = std::to_string(timeLeft);
    }
    int renewInterval = 0;
    if (FlatJson::GetInt(officeJson, "renew_interval", renewInterval)) {
        row.offRenewInterval = std::to_string(renewInterval);
    }
    return row;
}

} // namespace

std::string LocalDatabase::DefaultDbPath() {
    std::string dir = GetExecutableDirectory();
    if (dir.empty()) {
        return "";
    }
    return dir + "\\license_checker_results.db";
}

SaveResultOutcome LocalDatabase::SaveResult(const LicenseResult& result) {
    SaveResultOutcome outcome;

    std::string dbPath = DefaultDbPath();
    if (dbPath.empty()) {
        outcome.error = "Không xác định được thư mục chứa file thực thi.";
        return outcome;
    }

    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        outcome.error = "Không mở được database: " + std::string(db ? sqlite3_errmsg(db) : "unknown error");
        if (db) sqlite3_close(db);
        return outcome;
    }

    std::string error;
    bool ok = ExecSimple(db, kSchemaSql, error);

    std::string hostname = GetLocalHostname();
    if (hostname.empty()) {
        hostname = "Unknown";
    }
    std::string now = CurrentTimestampUtc();
    std::string osVersion = FormatOsVersion(result.GetWindowsVersion(), result.GetWindowsEdition());
    std::string windowsJson = BuildWindowsLicenseJson(result, now);
    std::string officeJson = BuildOfficeLicenseJson(result, now);
    std::string windowsStatus = NormalizeStatusCode(LicenseStatusToRealString(result.GetLicenseStatus()));
    std::string officeStatus = NormalizeStatusCode(OfficeStatusToRealString(result.GetOfficeLicenseInfo().licenseStatus));

    if (ok) ok = sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK;

    std::string siteId, sessionId;
    if (ok) {
        siteId = FindOrCreateSite(db, hostname, now, error);
        ok = !siteId.empty();
    }
    if (ok) {
        sessionId = FindOrCreateOrphanSession(db, siteId, now, error);
        ok = !sessionId.empty();
    }

    if (ok) {
        LastReportRow last = QueryLastReport(db, sessionId);
        bool isDuplicate = last.found &&
            SnapshotFromFields(windowsStatus, officeStatus, osVersion, windowsJson, officeJson) ==
                SnapshotFromFields(last.windowsStatus, last.officeStatus, last.osVersion, last.windowsJson, last.officeJson);

        if (!isDuplicate) {
            ok = InsertScannedMachine(db, sessionId, hostname, now, error);
            if (ok) {
                ok = InsertLicenseReport(db, sessionId, hostname, osVersion, windowsJson, officeJson, windowsStatus, officeStatus, now, error);
            }
            outcome.inserted = ok;
        }
    }

    if (ok) {
        ok = sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr) == SQLITE_OK;
        if (!ok) error = sqlite3_errmsg(db);
    } else {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    }

    sqlite3_close(db);
    if (!ok) {
        outcome.error = "Lỗi ghi database: " + error;
        outcome.inserted = false;
    }
    return outcome;
}

std::vector<ExcelReportRow> LocalDatabase::ListHistory(std::string* outError) {
    std::vector<ExcelReportRow> out;

    std::string dbPath = DefaultDbPath();
    if (dbPath.empty()) {
        if (outError) *outError = "Không xác định được thư mục chứa file thực thi.";
        return out;
    }
    if (GetFileAttributesA(dbPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return out; // no db file yet - nothing has been saved, not an error
    }

    sqlite3* db = nullptr;
    if (sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        if (outError) *outError = db ? sqlite3_errmsg(db) : "unknown error";
        if (db) sqlite3_close(db);
        return out;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT hostname, os_version, windows_status, office_status, windows_license_json, office_license_json, reported_at "
        "FROM license_reports ORDER BY reported_at DESC";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (outError) *outError = sqlite3_errmsg(db);
        sqlite3_close(db);
        return out;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto col = [&](int i) {
            const unsigned char* t = sqlite3_column_text(stmt, i);
            return t ? std::string(reinterpret_cast<const char*>(t)) : std::string();
        };
        out.push_back(RowFromStoredReport(col(0), col(1), col(2), col(3), col(4), col(5), col(6)));
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return out;
}
