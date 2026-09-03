#pragma once

#include <string>

// Minimal flat-JSON-object field extraction, scoped to exactly the JSON
// shape LocalDatabase itself generates (see BuildWindowsLicenseJson/
// BuildOfficeLicenseJson in LocalDatabase.cpp): a single-level object of
// string/bool/int values, no arrays, no nesting, no inter-token whitespace.
// Not a general JSON parser - only used to read our own previously-stored
// license_reports rows back into structured fields (for the dedup check on
// save, and for "Xem kết quả kiểm tra").
namespace FlatJson {
    bool GetString(const std::string& json, const std::string& key, std::string& outValue);
    bool GetBool(const std::string& json, const std::string& key, bool& outValue);
    bool GetInt(const std::string& json, const std::string& key, int& outValue);
}
