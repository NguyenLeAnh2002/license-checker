#pragma once

#include <string>
#include <vector>
#include "LicenseResult.h"
#include "ExcelReport.h"

// Outcome of a SaveResult() call.
struct SaveResultOutcome {
    // True if a new license_reports row was actually appended. False (with
    // Error still empty) means the result was identical to the most recent
    // saved report for this machine, so nothing new was written - the
    // "check trùng" half of "append with duplicate checking".
    bool inserted = false;
    std::string error; // non-empty on failure
};

// Persists a LicenseResult into a local SQLite file, schema-identical to
// license_checker_server's (minus that server's own local_config/merge_log
// tables, which are server-only concepts) - so the resulting .db file can
// be imported into a license_checker_server instance later through its
// existing "Gộp dữ liệu đa site" (DB-merge) feature, with zero server-side
// changes. Every save finds-or-creates one "site" (named after this
// machine's hostname) and one orphan session under it (mirroring the
// server's own "unassigned reports" concept), then appends one
// scanned_machines + one license_reports row - unless the result is
// identical to the last one saved, in which case nothing is written.
class LocalDatabase {
public:
    // Path of the db file this feature writes to: <exe-dir>\license_checker_results.db.
    // Exposed so the UI can show/offer it (e.g. in a "reveal in Explorer" action).
    static std::string DefaultDbPath();

    // Opens (creating if needed) DefaultDbPath(), ensures its schema, and
    // appends a new row for `result` - unless it's identical (same OS
    // version and Windows/Office status/detail fields, ignoring only the
    // check timestamp) to the most recent report already stored for this
    // machine, in which case it's skipped (see SaveResultOutcome::inserted).
    static SaveResultOutcome SaveResult(const LicenseResult& result);

    // Reads every license_reports row ever saved to DefaultDbPath() (newest
    // first), for "Xem kết quả kiểm tra". Returns an empty list (not an
    // error) if the db file doesn't exist yet - nothing has been saved.
    // On a real read failure, *outError is set (if outError is non-null).
    static std::vector<ExcelReportRow> ListHistory(std::string* outError = nullptr);
};
