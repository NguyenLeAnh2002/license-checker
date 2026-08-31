#include "ServerReporter.h"
#include "../license-detection/DetectionLogger.h"
#include "../license-detection/LicenseStatusEnum.h"
#include "../license-detection/LicenseResultMapping.h"
#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>
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

// Reserved trailer that license_checker_server (see internal/checkerstamp
// and scripts/AppendTrailer.ps1 in that repo) overwrites in place when it
// hosts this binary for download, embedding its own address so this agent
// needs no companion config file and no input from whoever runs it.
//
// This is deliberately NOT a compile-time struct anymore: this binary gets
// Authenticode-signed once, with no config block at all, and a one-time
// post-sign packaging step (AppendTrailer.ps1) appends this 536-byte block
// after the signature and grows the certificate table's declared size to
// cover it. That keeps the block entirely outside the region the
// Authenticode digest covers, so the controller can rewrite the payload in
// place indefinitely without ever invalidating the signature - a struct
// living inside the compiled .rdata section would sit inside the hashed
// region and break the signature on the very first re-stamp. The 536-byte
// size is load-bearing (must stay a multiple of 8 - see AppendTrailer.ps1);
// don't change it without re-running that verification.
//
// Because the block isn't part of the compiled image, it can't be read as
// an in-memory struct - ReadEmbeddedServerUrl() below re-opens this
// process's own .exe file on disk and reads the trailing bytes directly.
// Layout, marker text, and payload size are a fixed contract with that
// repo - if any of these change, the two must change together.
const char kBeginMarker[] = "LCCFG_BEGIN_V1";
const char kEndMarker[]   = "LCCFG_END_V1";
constexpr size_t kMarkerSize  = 16;
constexpr size_t kPayloadSize = 504;
constexpr size_t kBlockSize   = kMarkerSize + kPayloadSize + kMarkerSize;  // 536

bool MatchesPaddedMarker(const char* data, const char* marker) {
    char padded[kMarkerSize] = {0};
    strncpy(padded, marker, kMarkerSize);
    return memcmp(data, padded, kMarkerSize) == 0;
}

// Opens this binary's own .exe file on disk (not the in-memory image,
// which never contains this trailer) and reads the server URL from the
// block appended after it was signed. Returns "" if the trailer is
// missing, corrupted, or on any I/O error - identical to the "not stamped
// yet" case, so LoadConfig() falls back the same way either way.
//
// Opening the very file this process is currently executing from is safe
// on Windows: the loader holds only a FILE_SHARE_READ image section, not
// an exclusive handle, so a plain read-only open alongside it works.
std::string ReadEmbeddedServerUrl() {
    char pathBuf[MAX_PATH];
    DWORD pathLen = GetModuleFileNameA(NULL, pathBuf, MAX_PATH);
    if (pathLen == 0 || pathLen == MAX_PATH) {
        return "";
    }

    std::ifstream file(std::string(pathBuf, pathLen), std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return "";
    }
    std::streamoff fileSize = file.tellg();
    if (fileSize < static_cast<std::streamoff>(kBlockSize)) {
        return "";
    }

    file.seekg(fileSize - static_cast<std::streamoff>(kBlockSize));
    char block[kBlockSize];
    if (!file.read(block, kBlockSize)) {
        return "";
    }

    if (!MatchesPaddedMarker(block, kBeginMarker)) {
        return "";
    }
    if (!MatchesPaddedMarker(block + kMarkerSize + kPayloadSize, kEndMarker)) {
        return "";
    }

    char buf[kPayloadSize + 1];
    memcpy(buf, block + kMarkerSize, kPayloadSize);
    buf[kPayloadSize] = '\0';
    return std::string(buf);
}

// Translates the handful of WinHTTP error codes actually seen in the field
// (see winhttp.h for the full list) so the log is self-explanatory without
// needing to look the number up. Falls back to just the raw code for
// anything not covered here.
std::string DescribeWinHttpError(DWORD code) {
    switch (code) {
        case ERROR_WINHTTP_TIMEOUT:              return "timeout";
        case ERROR_WINHTTP_NAME_NOT_RESOLVED:    return "name not resolved (thường do proxy WinHTTP cấu hình sai - xem 'netsh winhttp show proxy', không phải do DNS/mạng của địa chỉ đích)";
        case ERROR_WINHTTP_CANNOT_CONNECT:       return "cannot connect (máy chủ từ chối kết nối / sai cổng / firewall chặn)";
        case ERROR_WINHTTP_CONNECTION_ERROR:     return "connection error (kết nối bị reset giữa chừng)";
        case ERROR_WINHTTP_SECURE_FAILURE:       return "TLS/SSL handshake failed";
        case ERROR_WINHTTP_INVALID_URL:          return "invalid server_url";
        case ERROR_WINHTTP_UNRECOGNIZED_SCHEME:  return "server_url thiếu hoặc sai scheme (phải là http:// hoặc https://)";
        default:                                 return "unknown WinHTTP error";
    }
}

} // namespace

ServerReporter::ServerReporter(DetectionLogger& logger)
    : logger_(logger) {
    config_ = LoadConfig();
    if (config_.enabled) {
        logger_.LogInfo("ServerReporter: reporting enabled, server=" + config_.serverUrl);
    } else {
        logger_.LogInfo("ServerReporter: reporting disabled (no server address embedded in this binary and no agent_config.json next to the executable)");
    }
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

    // Primary path: an address stamped directly into this binary by
    // license_checker_server at upload time (see EmbeddedServerConfig
    // above) - needs no companion file and no input from whoever runs it.
    std::string embeddedUrl = ReadEmbeddedServerUrl();
    if (!embeddedUrl.empty()) {
        logger_.LogInfo("ServerReporter: found embedded server address: " + embeddedUrl);
        config.serverUrl = embeddedUrl;
        config.enabled = true;
    } else {
        logger_.LogInfo("ServerReporter: no embedded server address in this binary (unstamped build, or this download predates that feature)");
    }

    // agent_config.json next to the executable, if present, is an optional
    // manual override on top of the embedded address - lets a companion
    // file still set/replace the api_key, or replace server_url entirely
    // for manual/dev setups that predate stamping.
    std::string dir = GetExecutableDirectory();
    if (dir.empty()) {
        logger_.LogInfo("ServerReporter: GetModuleFileNameA failed (error " + std::to_string(GetLastError()) + ") - cannot locate agent_config.json");
        return config;
    }
    std::string configPath = dir + "\\agent_config.json";

    std::ifstream file(configPath, std::ios::binary);
    if (!file.is_open()) {
        logger_.LogInfo("ServerReporter: no agent_config.json at " + configPath + " (fine if an address is already embedded above)");
        return config;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    logger_.LogInfo("ServerReporter: found agent_config.json at " + configPath + " (" + std::to_string(content.size()) + " bytes)");

    // Accept both this project's mock-server config schema (server_url/
    // api_key) and the real BE server's default config field names
    // (serverUrl/X-Api-Key), since installs get configs generated by either.
    std::string serverUrl;
    if (((TryGetJsonString(content, "server_url", serverUrl) && !serverUrl.empty()) ||
         (TryGetJsonString(content, "serverUrl", serverUrl) && !serverUrl.empty()))) {
        logger_.LogInfo("ServerReporter: agent_config.json overrides server address: " + serverUrl);
        config.serverUrl = serverUrl;
        config.enabled = true;
    } else if (!config.enabled) {
        logger_.LogInfo("ServerReporter: agent_config.json has no server_url/serverUrl field, and no embedded address either - reporting stays disabled");
        return config;
    }

    std::string apiKey;
    if ((TryGetJsonString(content, "api_key", apiKey) && !apiKey.empty()) ||
        (TryGetJsonString(content, "X-Api-Key", apiKey) && !apiKey.empty()) ||
        TryGetJsonString(content, "apiKey", apiKey)) {
        config.apiKey = apiKey;
        logger_.LogInfo("ServerReporter: agent_config.json provides an api_key (" + std::to_string(apiKey.size()) + " chars)");
    }

    return config;
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
        DWORD err = GetLastError();
        outError = "Failed to parse server_url (error " + std::to_string(err) + " - " + DescribeWinHttpError(err) + ")";
        return false;
    }

    bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    INTERNET_PORT port = urlComp.nPort;

    std::wstring path = pathBuf[0] ? pathBuf : L"/";
    if (!path.empty() && path.back() == L'/') {
        path.pop_back();
    }
    path += L"/api/report";

    // The controller is always a LAN address (see AGENT_SERVER_PROTOCOL_REAL.md/
    // the mobile-controller design), never something that should go through a
    // corporate web proxy. WINHTTP_ACCESS_TYPE_DEFAULT_PROXY uses the
    // machine-wide WinHTTP proxy setting (netsh winhttp show proxy) - separate
    // from whatever the browser uses - and on machines where that's configured,
    // WinHTTP tries to resolve the *proxy's* hostname before ever reaching the
    // target, failing with ERROR_WINHTTP_NAME_NOT_RESOLVED (12007) even though
    // the LAN target itself is perfectly reachable (ping/browser work fine,
    // since neither of those goes through this proxy setting). Force a direct
    // connection instead.
    HINTERNET hSession = WinHttpOpen(L"LicenseCheckerUI/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        DWORD err = GetLastError();
        outError = "WinHttpOpen failed (error " + std::to_string(err) + " - " + DescribeWinHttpError(err) + ")";
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
        DWORD err = GetLastError();
        outError = "WinHttpConnect failed (error " + std::to_string(err) + " - " + DescribeWinHttpError(err) + ")";
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        DWORD err = GetLastError();
        outError = "WinHttpOpenRequest failed (error " + std::to_string(err) + " - " + DescribeWinHttpError(err) + ")";
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
        DWORD err = GetLastError();
        outError = "send/receive failed (error " + std::to_string(err) + " - " + DescribeWinHttpError(err) + ")";
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

        // Field testing found some machines' first attempt in a run succeeds,
        // but a retry moments later against the exact same address fails with
        // the identical WinHTTP error, then later succeeds again - consistent
        // with a transient local resolver/network-security-software hiccup
        // rather than a hard, consistent block (which would fail every time).
        // A couple of quick retries absorb that instead of losing the whole
        // cycle's report and having to wait for the next scheduled one.
        const int maxAttempts = 3;
        const DWORD retryDelayMs = 2000;
        std::string error;
        for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
            if (HttpPost(payload, error)) {
                logger_.LogInfo("ServerReporter: report sent successfully to " + config_.serverUrl +
                    (attempt > 1 ? " (attempt " + std::to_string(attempt) + "/" + std::to_string(maxAttempts) + ")" : ""));
                return;
            }
            logger_.LogFailure("ServerReporter: attempt " + std::to_string(attempt) + "/" +
                std::to_string(maxAttempts) + " failed - " + error);
            if (attempt < maxAttempts) {
                Sleep(retryDelayMs);
            }
        }
        logger_.LogFailure("ServerReporter: giving up after " + std::to_string(maxAttempts) + " attempts - " + error);
    } catch (const std::exception& ex) {
        logger_.LogFailure(std::string("ServerReporter: exception while sending report - ") + ex.what());
    } catch (...) {
        logger_.LogFailure("ServerReporter: unknown exception while sending report");
    }
}
