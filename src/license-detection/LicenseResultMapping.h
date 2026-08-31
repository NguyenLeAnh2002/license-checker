#pragma once

#include <string>
#include "LicenseStatusEnum.h"

// Shared field-mapping helpers between a LicenseResult and the vocabulary
// license_checker_server expects (see AGENT_SERVER_PROTOCOL_REAL.md) - used
// by ServerReporter (HTTP reporting), LocalDatabase ("Lưu vào database"),
// and ExcelReport ("Xuất file Excel") so all three ways of persisting a
// check result agree on hostname/OS/status formatting. Originally lived
// only in ServerReporter.cpp; pulled out here so the newer local-persistence
// features don't have to duplicate (and risk drifting from) the same
// mapping logic.

// Vocabulary for the BE team's real server - all four values confirmed:
//   Legitimate         -> "VALID"
//   Cracked            -> "CRACKED"
//   NotLicensed        -> "NOT_ACTIVATED"
//   UnableToDetermine  -> "UNKNOWN"
std::string LicenseStatusToRealString(LicenseStatus status);

// Office has no clean LicenseStatus-style enum in this codebase - it's the
// raw "LICENSE STATUS: ---XXX---" token from `cscript ospp.vbs /dstatus`
// (see OfficeOSPPDetector.cpp). Maps the standard OSPP tokens onto the same
// confirmed vocabulary as LicenseStatusToRealString().
std::string OfficeStatusToRealString(const std::string& rawOsppStatus);

// Lowercases the "VALID"/"CRACKED"/"NOT_ACTIVATED"/"UNKNOWN" wire vocabulary
// into the status code license_checker_server actually stores in its
// license_reports.windows_status/office_status columns (see
// model.NormalizeStatusCode in that repo) - "valid"/"cracked"/
// "not_activated"/"unknown". Any value outside that set also maps to
// "unknown", matching the server's own fallback behavior.
std::string NormalizeStatusCode(const std::string& realString);

std::string GetLocalHostname();

// Maps LicenseResult::GetWindowsVersion()'s numeric code (7/8/81/10/11) plus
// the detected edition into the human-readable "os_version" string the
// server expects (e.g. "Windows 10 Pro").
std::string FormatOsVersion(int version, const std::string& edition);

std::string CurrentTimestampUtc();

// Extracts a leading integer from strings like "174 minute(s)" (as reported
// by `cscript ospp.vbs`). Returns false (leaving outValue untouched) if no
// digits are found, so callers can distinguish "0" from "not present".
bool ParseLeadingInt(const std::string& s, int& outValue);

std::string JsonEscape(const std::string& value);

// Renders a normalized status code ("valid"/"cracked"/"not_activated"/
// "unknown") as the same Vietnamese text license_checker_server's own
// Excel export uses (see model.StatusLabel in that repo), so a report
// produced locally reads identically to one produced by the server.
std::string StatusLabelVi(const std::string& statusCode);

// Resolves the directory this process's own .exe lives in via
// GetModuleFileNameA(NULL, ...) - independent of the current working
// directory, which isn't reliably the exe's folder (a shortcut with a
// different "Start in", a scheduled task, etc). Returns "" on failure.
std::string GetExecutableDirectory();
