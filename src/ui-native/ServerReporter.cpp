#include "ServerReporter.h"
#include "../license-detection/DetectionLogger.h"
#include "../license-detection/LicenseStatusEnum.h"
#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cctype>
#include <ctime>
#include <chrono>
#include <cwchar>

#pragma comment(lib, "winhttp.lib")

namespace {

std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return std::wstring();
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    if (size <= 0) {
        return std::wstring();
    }
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], size);
    return result;
}

// Vocabulary for the BE team's real server - all four values confirmed:
//   Legitimate         -> "VALID"
//   Cracked            -> "CRACKED"
//   NotLicensed        -> "NOT_ACTIVATED"
//   UnableToDetermine  -> "UNKNOWN"
std::string LicenseStatusToRealString(LicenseStatus status) {
    switch (status) {
        case LicenseStatus::Legitimate:       return "VALID";
        case LicenseStatus::Cracked:          return "CRACKED";
        case LicenseStatus::NotLicensed:      return "NOT_ACTIVATED";
        case LicenseStatus::UnableToDetermine:
        default:                             return "UNKNOWN";
    }
}

// Office has no clean LicenseStatus-style enum in this codebase - it's the
// raw "LICENSE STATUS: ---XXX---" token from `cscript ospp.vbs /dstatus`
// (see OfficeOSPPDetector.cpp). Map the standard OSPP tokens onto the same
// confirmed vocabulary as LicenseStatusToRealString() (NOT_ACTIVATED/
// CRACKED/VALID/UNKNOWN). The OSPP-token-to-category mapping itself
// (which raw tokens mean "cracked" vs "grace period" etc.) is still a
// best-effort heuristic - not confirmed against the real backend.
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

std::string GetLocalHostname() {
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buffer);
    if (GetComputerNameA(buffer, &size)) {
        return std::string(buffer, size);
    }
    return "";
}

// Maps LicenseResult::GetWindowsVersion()'s numeric code (7/8/81/10/11) plus
// the detected edition into the human-readable "os_version" string the
// server expects (e.g. "Windows 10 Pro").
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

// Extracts a leading integer from strings like "174 minute(s)" (as reported
// by `cscript ospp.vbs`). Returns false (leaving outValue untouched) if no
// digits are found, so callers can distinguish "0" from "not present".
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

} // namespace

ServerReporter::ServerReporter(DetectionLogger& logger)
    : logger_(logger) {
    config_ = LoadConfig();
    if (config_.enabled) {
        logger_.LogInfo("ServerReporter: reporting enabled, server=" + config_.serverUrl);
    } else {
        logger_.LogInfo("ServerReporter: reporting disabled (no agent_config.json with a usable server_url/api_key next to the executable)");
    }
}

std::string ServerReporter::GetExecutableDirectory() {
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

bool ServerReporter::TryGetJsonString(const std::string& json, const std::string& key, std::string& outValue) {
    std::string pattern = "\"" + key + "\"";
    size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) {
        return false;
    }
    size_t colonPos = json.find(':', keyPos + pattern.size());
    if (colonPos == std::string::npos) {
        return false;
    }
    size_t quoteStart = json.find('"', colonPos);
    if (quoteStart == std::string::npos) {
        return false;
    }

    std::string value;
    size_t searchPos = quoteStart + 1;
    while (searchPos < json.size() && json[searchPos] != '"') {
        if (json[searchPos] == '\\' && searchPos + 1 < json.size()) {
            searchPos++;
        }
        value.push_back(json[searchPos]);
        searchPos++;
    }

    outValue = value;
    return true;
}

ServerReporter::Config ServerReporter::LoadConfig() const {
    Config config;

    std::string dir = GetExecutableDirectory();
    if (dir.empty()) {
        return config;
    }
    std::string configPath = dir + "\\agent_config.json";

    std::ifstream file(configPath, std::ios::binary);
    if (!file.is_open()) {
        return config;  // No config file - reporting stays disabled.
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // No separate "enabled" flag: reporting is on whenever a usable
    // destination + credential are present below. Accept both this
    // project's mock-server config schema (server_url/api_key) and the
    // real BE server's default config field names (serverUrl/X-Api-Key),
    // since installs get configs generated by either.
    std::string serverUrl;
    if ((!TryGetJsonString(content, "server_url", serverUrl) || serverUrl.empty()) &&
        (!TryGetJsonString(content, "serverUrl", serverUrl) || serverUrl.empty())) {
        return config;  // Can't report without a destination.
    }
    config.serverUrl = serverUrl;

    std::string apiKey;
    if ((!TryGetJsonString(content, "api_key", apiKey) || apiKey.empty()) &&
        (!TryGetJsonString(content, "X-Api-Key", apiKey) || apiKey.empty())) {
        TryGetJsonString(content, "apiKey", apiKey);
    }
    config.apiKey = apiKey;

    config.enabled = true;
    return config;
}

std::string ServerReporter::JsonEscape(const std::string& value) {
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

// Payload shape matches the controller's POST /api/report handler
// (internal/httpserver/public.go / internal/model/license_status.go in the
// license_checker_server repo) - see AGENT_SERVER_PROTOCOL_REAL.md. "ip" is
// left blank intentionally: the server falls back to the request's source
// IP, which is also what it uses to match a machine's own results back to
// it on the landing page, so a self-reported IP would risk a mismatch.
std::string ServerReporter::BuildPayload(const LicenseResult& result) const {
    const auto& windowsInfo = result.GetWindowsLicenseInfo();
    const auto& officeInfo = result.GetOfficeLicenseInfo();

    std::string hostname = GetLocalHostname();
    std::string osVersion = FormatOsVersion(result.GetWindowsVersion(), result.GetWindowsEdition());
    std::string checkTime = CurrentTimestampUtc();
    bool hasKmsServer = !result.GetWindowsKmsServer().empty();

    std::ostringstream windowsJson;
    windowsJson << "{"
        << "\"name\":\"" << JsonEscape(windowsInfo.name) << "\","
        << "\"product_name\":\"" << JsonEscape(result.GetWindowsEdition()) << "\","
        << "\"description\":\"" << JsonEscape(windowsInfo.description) << "\","
        << "\"license_status\":\"" << LicenseStatusToRealString(result.GetLicenseStatus()) << "\","
        << "\"partial_key\":\"" << JsonEscape(windowsInfo.partialProductKey) << "\","
        << "\"kms\":" << (hasKmsServer ? "true" : "false") << ","
        << "\"kms_server\":\"" << JsonEscape(result.GetWindowsKmsServer()) << "\","
        << "\"check_time\":\"" << checkTime << "\""
        << "}";

    std::ostringstream officeJson;
    officeJson << "{"
        << "\"license_status\":\"" << OfficeStatusToRealString(officeInfo.licenseStatus) << "\","
        << "\"product_name\":\"" << JsonEscape(officeInfo.licenseName) << "\","
        << "\"partial_key\":\"" << JsonEscape(officeInfo.partialProductKey) << "\","
        << "\"kms_server\":\"" << JsonEscape(officeInfo.kmsServer) << "\",";

    int timeLeft = 0;
    ParseLeadingInt(officeInfo.remainingGrace, timeLeft);
    officeJson << "\"time_left\":" << timeLeft << ",";

    int renewInterval = 0;
    if (ParseLeadingInt(officeInfo.renewalInterval, renewInterval)) {
        officeJson << "\"renew_interval\":" << renewInterval << ",";
    }
    officeJson << "\"check_time\":\"" << checkTime << "\""
        << "}";

    std::ostringstream json;
    json << "{"
        << "\"hostname\":\"" << JsonEscape(hostname) << "\","
        << "\"ip\":\"\","
        << "\"os_version\":\"" << JsonEscape(osVersion) << "\","
        << "\"windows_license\":" << windowsJson.str() << ","
        << "\"office_license\":" << officeJson.str()
        << "}";
    return json.str();
}

bool ServerReporter::HttpPost(const std::string& jsonBody, std::string& outError) const {
    URL_COMPONENTSW urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostBuf[256] = {0};
    wchar_t pathBuf[1024] = {0};
    urlComp.lpszHostName = hostBuf;
    urlComp.dwHostNameLength = _countof(hostBuf);
    urlComp.lpszUrlPath = pathBuf;
    urlComp.dwUrlPathLength = _countof(pathBuf);

    std::wstring wideUrl = Utf8ToWide(config_.serverUrl);
    if (!WinHttpCrackUrl(wideUrl.c_str(), (DWORD)wideUrl.size(), 0, &urlComp)) {
        outError = "Failed to parse server_url (error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    INTERNET_PORT port = urlComp.nPort;

    std::wstring path = pathBuf[0] ? pathBuf : L"/";
    if (!path.empty() && path.back() == L'/') {
        path.pop_back();
    }
    path += L"/api/report";

    HINTERNET hSession = WinHttpOpen(L"LicenseCheckerUI/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        outError = "WinHttpOpen failed (error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    // Bounded timeouts so an unreachable/hung server can't stall a detection cycle.
    WinHttpSetTimeouts(hSession, 5000, 5000, 15000, 15000);

    // Windows 7's WinHTTP defaults to SSL3/TLS1.0 only - a modern HTTPS
    // server (e.g. checker.vnpt.vn) rejects that handshake outright
    // (ERROR_WINHTTP_SECURE_FAILURE / 12175). Explicitly opt into TLS1.1/1.2
    // so this works on Win7 SP1 without relying on OS-level registry tweaks;
    // harmless no-op on newer Windows where these are already the default.
    DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols));

    HINTERNET hConnect = WinHttpConnect(hSession, hostBuf, port, 0);
    if (!hConnect) {
        outError = "WinHttpConnect failed (error " + std::to_string(GetLastError()) + ")";
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        outError = "WinHttpOpenRequest failed (error " + std::to_string(GetLastError()) + ")";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wstring contentTypeHeader = L"Content-Type: application/json; charset=utf-8";
    WinHttpAddRequestHeaders(hRequest, contentTypeHeader.c_str(), (DWORD)-1,
        WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    // The controller's /api/report endpoint doesn't check this header, but
    // send it when configured in case a future server revision starts to.
    if (!config_.apiKey.empty()) {
        std::wstring apiKeyHeader = L"X-Api-Key: " + Utf8ToWide(config_.apiKey);
        WinHttpAddRequestHeaders(hRequest, apiKeyHeader.c_str(), (DWORD)-1,
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    // Log the exact outgoing request (method, full URL, headers, body) so
    // it can be compared directly against a known-working curl/PowerShell
    // request when debugging server-side rejections.
    {
        // Rebuild host[:port] + path explicitly rather than trusting serverUrl's
        // formatting, so this reflects what WinHTTP is actually connecting to.
        std::string hostStr(hostBuf, hostBuf + wcslen(hostBuf));
        std::string pathStr(path.begin(), path.end());
        bool isDefaultPort = (!isHttps && port == 80) || (isHttps && port == 443);
        std::ostringstream fullUrl;
        fullUrl << (isHttps ? "https://" : "http://") << hostStr;
        if (!isDefaultPort) {
            fullUrl << ":" << port;
        }
        fullUrl << pathStr;

        logger_.LogInfo("ServerReporter: sending POST " + fullUrl.str());
        logger_.LogInfo("ServerReporter: headers - Content-Type: application/json; charset=utf-8" +
            (config_.apiKey.empty() ? "" : (" | X-Api-Key: " + config_.apiKey)));
        logger_.LogInfo("ServerReporter: body - " + jsonBody);
    }

    BOOL sent = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)jsonBody.data(), (DWORD)jsonBody.size(), (DWORD)jsonBody.size(), 0);

    bool success = false;
    if (sent && WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

        if (statusCode >= 200 && statusCode < 300) {
            success = true;
        } else {
            std::string body;
            DWORD available = 0;
            while (WinHttpQueryDataAvailable(hRequest, &available) && available > 0) {
                std::string chunk(available, '\0');
                DWORD read = 0;
                if (!WinHttpReadData(hRequest, &chunk[0], available, &read)) {
                    break;
                }
                body.append(chunk, 0, read);
                if (body.size() > 2048) {
                    break;  // Enough for a diagnostic message.
                }
            }
            outError = "server responded with HTTP " + std::to_string(statusCode) +
                (body.empty() ? "" : (" - " + body));
        }
    } else {
        outError = "send/receive failed (error " + std::to_string(GetLastError()) + ")";
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return success;
}

void ServerReporter::SendReport(const LicenseResult& result) {
    if (!config_.enabled) {
        return;
    }

    try {
        std::string payload = BuildPayload(result);
        std::string error;
        if (HttpPost(payload, error)) {
            logger_.LogInfo("ServerReporter: report sent successfully to " + config_.serverUrl);
        } else {
            logger_.LogFailure("ServerReporter: failed to send report - " + error);
        }
    } catch (const std::exception& ex) {
        logger_.LogFailure(std::string("ServerReporter: exception while sending report - ") + ex.what());
    } catch (...) {
        logger_.LogFailure("ServerReporter: unknown exception while sending report");
    }
}
