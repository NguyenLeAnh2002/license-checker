#include "LicenseResultMapping.h"

#include <windows.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

std::string LicenseStatusToRealString(LicenseStatus status) {
    switch (status) {
        case LicenseStatus::Legitimate:       return "VALID";
        case LicenseStatus::Cracked:          return "CRACKED";
        case LicenseStatus::NotLicensed:      return "NOT_ACTIVATED";
        case LicenseStatus::UnableToDetermine:
        default:                             return "UNKNOWN";
    }
}

std::string OfficeStatusToRealString(const std::string& rawOsppStatus) {
    if (rawOsppStatus.empty()) {
        return "UNKNOWN";
    }
    if (rawOsppStatus.find("NON_GENUINE") != std::string::npos) {
        return "CRACKED";
    }
    if (rawOsppStatus.find("LICENSED") != std::string::npos) {
        return "VALID";
    }
    if (rawOsppStatus.find("GRACE") != std::string::npos ||
        rawOsppStatus.find("NOTIFICATIONS") != std::string::npos ||
        rawOsppStatus.find("HOLD") != std::string::npos) {
        return "NOT_ACTIVATED";
    }
    return "UNKNOWN";
}

std::string NormalizeStatusCode(const std::string& realString) {
    if (realString == "VALID") return "valid";
    if (realString == "CRACKED") return "cracked";
    if (realString == "NOT_ACTIVATED") return "not_activated";
    return "unknown";
}

std::string GetLocalHostname() {
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buffer);
    if (GetComputerNameA(buffer, &size)) {
        return std::string(buffer, size);
    }
    return "";
}

std::string FormatOsVersion(int version, const std::string& edition) {
    std::string base;
    switch (version) {
        case 7:  base = "Windows 7"; break;
        case 8:  base = "Windows 8"; break;
        case 81: base = "Windows 8.1"; break;
        case 10: base = "Windows 10"; break;
        case 11: base = "Windows 11"; break;
        default: base = "Windows"; break;
    }
    if (!edition.empty()) {
        base += " " + edition;
    }
    return base;
}

std::string CurrentTimestampUtc() {
    std::time_t now = std::time(nullptr);
    std::tm utcTm{};
    gmtime_s(&utcTm, &now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utcTm);
    return std::string(buf);
}

bool ParseLeadingInt(const std::string& s, int& outValue) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        i++;
    }
    bool negative = false;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) {
        negative = (s[i] == '-');
        i++;
    }
    size_t digitsStart = i;
    long value = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        value = value * 10 + (s[i] - '0');
        i++;
    }
    if (i == digitsStart) {
        return false;
    }
    outValue = negative ? -static_cast<int>(value) : static_cast<int>(value);
    return true;
}

std::string JsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string StatusLabelVi(const std::string& statusCode) {
    if (statusCode == "valid") return "Hợp lệ";
    if (statusCode == "cracked") return "Bẻ khóa (Cracked)";
    if (statusCode == "not_activated") return "Chưa kích hoạt";
    return "Chưa xác định";
}

std::string GetExecutableDirectory() {
    char pathBuf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, pathBuf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return "";
    }
    std::string path(pathBuf, len);
    size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return "";
    }
    return path.substr(0, pos);
}
