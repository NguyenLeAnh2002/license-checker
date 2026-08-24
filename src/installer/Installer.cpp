// Installer for the License Checker agent.
//
// Run from a folder containing LicenseCheckerAgent.exe, agent_config.json,
// and (optionally) LicenseCheckerUI.exe - exactly what the server's
// install-kit ZIP download bundles together. Copies those into
// C:\LicenseChecker, registers the machine with the configured server via
// POST /api/agent/register, then installs and starts LicenseCheckerAgent
// as an auto-start Windows service (which handles ongoing license
// reporting to /api/agent/report-license on its own).
//
// Requires administrator privileges (see Installer.vcxproj's
// UACExecutionLevel) - both installing a service and writing to C:\ need them.

#include "../agent/ServiceInstaller.h"
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <initializer_list>
#include <filesystem>
#include <vector>
#include <cstring>

#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;

namespace {

const char* kServiceName = "LicenseCheckerAgent";
const char* kDisplayName = "License Checker Agent";
const char* kInstallDir = "C:\\LicenseChecker";

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

// Minimal fixed-shape JSON string extraction - same approach as
// ServerReporter::TryGetJsonString, kept local since agent_config.json's
// shape is small and well-known (not general-purpose JSON parsing).
bool TryGetJsonString(const std::string& json, const std::string& key, std::string& outValue) {
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

// Tries each candidate key in order, returning the first non-empty match.
// Config files may come from either this project's mock server (server_url/
// api_key) or the real BE server's default output (serverUrl/X-Api-Key) -
// accept both rather than requiring one fixed schema.
bool TryGetJsonStringAny(const std::string& json, std::initializer_list<const char*> keys, std::string& outValue) {
    for (const char* key : keys) {
        std::string value;
        if (TryGetJsonString(json, key, value) && !value.empty()) {
            outValue = value;
            return true;
        }
    }
    return false;
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

std::string GetLocalHostname() {
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buffer);
    if (GetComputerNameA(buffer, &size)) {
        return std::string(buffer, size);
    }
    return "Unknown";
}

std::string GetLocalMachineGuid() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return "";
    }
    char guidBuf[256];
    DWORD size = sizeof(guidBuf);
    std::string result;
    if (RegQueryValueExA(hKey, "MachineGuid", NULL, NULL, (LPBYTE)guidBuf, &size) == ERROR_SUCCESS) {
        size_t len = (size > 0 && guidBuf[size - 1] == '\0') ? size - 1 : size;
        result.assign(guidBuf, len);
    }
    RegCloseKey(hKey);
    return result;
}

bool CopyRequiredFile(const std::string& srcDir, const std::string& destDir, const std::string& filename, bool required) {
    std::string src = srcDir + "\\" + filename;
    std::string dest = destDir + "\\" + filename;

    if (!fs::exists(src)) {
        if (required) {
            std::cerr << "ERROR: required file not found: " << src << std::endl;
            return false;
        }
        std::cout << "WARNING: optional file not found, skipping: " << src << std::endl;
        return true;
    }

    // On a reinstall, the previous service instance may take a moment to
    // fully exit after being stopped (ControlService() returning success
    // only means the stop request was accepted, not that the process has
    // exited yet) - retry past the resulting sharing violation instead of
    // relying on a single fixed delay before this call.
    const int maxAttempts = 10;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        if (CopyFileA(src.c_str(), dest.c_str(), FALSE)) {
            std::cout << "Copied " << filename << " -> " << dest << std::endl;
            return true;
        }
        DWORD err = GetLastError();
        if (err != ERROR_SHARING_VIOLATION || attempt == maxAttempts) {
            std::cerr << "ERROR: failed to copy " << filename << " (error " << err << ")" << std::endl;
            return false;
        }
        Sleep(500);
    }
    return false;
}

// POST {serverUrl}/api/agent/register with X-Api-Key + {hostname, machineGuid}.
// One-shot, best-effort: a failure here doesn't block installation - the
// agent service, once running, keeps reporting via /api/agent/report-license
// on its own regular cycle regardless.
bool RegisterMachine(const std::string& serverUrl, const std::string& apiKey) {
    if (serverUrl.empty() || apiKey.empty()) {
        std::cout << "Skipping /api/agent/register call - server_url or api_key missing from agent_config.json" << std::endl;
        return false;
    }

    std::string hostname = GetLocalHostname();
    std::string machineGuid = GetLocalMachineGuid();

    std::ostringstream json;
    json << "{\"hostname\":\"" << JsonEscape(hostname) << "\","
         << "\"machineGuid\":\"" << JsonEscape(machineGuid) << "\"}";
    std::string jsonBody = json.str();

    URL_COMPONENTSW urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostBuf[256] = { 0 };
    wchar_t pathBuf[1024] = { 0 };
    urlComp.lpszHostName = hostBuf;
    urlComp.dwHostNameLength = _countof(hostBuf);
    urlComp.lpszUrlPath = pathBuf;
    urlComp.dwUrlPathLength = _countof(pathBuf);

    std::wstring wideUrl = Utf8ToWide(serverUrl);
    if (!WinHttpCrackUrl(wideUrl.c_str(), (DWORD)wideUrl.size(), 0, &urlComp)) {
        std::cerr << "Registration skipped: failed to parse server_url (error " << GetLastError() << ")" << std::endl;
        return false;
    }

    bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    INTERNET_PORT port = urlComp.nPort;

    std::wstring path = pathBuf[0] ? pathBuf : L"/";
    if (!path.empty() && path.back() == L'/') {
        path.pop_back();
    }
    path += L"/api/agent/register";

    HINTERNET hSession = WinHttpOpen(L"LicenseCheckerInstaller/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        std::cerr << "Registration failed: WinHttpOpen error " << GetLastError() << std::endl;
        return false;
    }
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
        std::cerr << "Registration failed: WinHttpConnect error " << GetLastError() << std::endl;
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        std::cerr << "Registration failed: WinHttpOpenRequest error " << GetLastError() << std::endl;
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wstring contentTypeHeader = L"Content-Type: application/json";
    WinHttpAddRequestHeaders(hRequest, contentTypeHeader.c_str(), (DWORD)-1,
        WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    std::wstring apiKeyHeader = L"X-Api-Key: " + Utf8ToWide(apiKey);
    WinHttpAddRequestHeaders(hRequest, apiKeyHeader.c_str(), (DWORD)-1,
        WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    std::cout << "Registering machine: POST " << serverUrl << "/api/agent/register" << std::endl;
    std::cout << "  Body: " << jsonBody << std::endl;

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
            std::cout << "Machine registered successfully (HTTP " << statusCode << ")" << std::endl;
            success = true;
        } else {
            std::cerr << "Registration failed: server responded with HTTP " << statusCode << std::endl;
        }
    } else {
        std::cerr << "Registration failed: send/receive error " << GetLastError() << std::endl;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
}

// Launches the just-installed UI so the user immediately sees a result
// instead of having to go find LicenseCheckerUI.exe themselves. Best-effort:
// missing UI (optional component) or a launch failure isn't fatal.
void LaunchInstalledUI(const std::string& installDir) {
    std::string uiPath = installDir + "\\LicenseCheckerUI.exe";
    if (!fs::exists(uiPath)) {
        return;
    }

    std::string cmdLine = "\"" + uiPath + "\"";
    std::vector<char> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back('\0');

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    Sleep(2000);
    if (CreateProcessA(uiPath.c_str(), cmdLineBuf.data(), NULL, NULL, FALSE, 0,
                        NULL, installDir.c_str(), &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        std::cout << "Opened LicenseCheckerUI.exe" << std::endl;
    } else {
        std::cerr << "WARNING: failed to open the UI automatically (error " << GetLastError() << ")" << std::endl;
    }
}

// Removes the original install-kit files (the ones this Installer.exe was
// run from) once everything has been copied into installDir, the service is
// running, and the machine registered - the kit, including agent_config.json
// with its API key, no longer needs to sit around in Downloads/Desktop.
// The already-installed copies in installDir are never touched.
void CleanupInstallKit(const std::string& srcDir, const std::string& installDir) {
    // Safety: if this installer is somehow being run from inside installDir
    // itself (e.g. re-run directly from C:\LicenseChecker), srcDir and
    // installDir are the same place - deleting "the kit" there would delete
    // the files the service just started from. Skip cleanup entirely.
    if (_stricmp(srcDir.c_str(), installDir.c_str()) == 0) {
        return;
    }

    for (const char* name : { "LicenseCheckerAgent.exe", "LicenseCheckerUI.exe", "agent_config.json" }) {
        std::string path = srcDir + "\\" + name;
        if (fs::exists(path)) {
            DeleteFileA(path.c_str());
        }
    }
    std::cout << "Removed original install kit files from " << srcDir << std::endl;

    // Installer.exe can't delete its own running image directly (the file is
    // still open/locked by this process). Spawn a short-lived detached helper
    // that waits a moment for this process to exit, then deletes it.
    char selfPath[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, selfPath, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return;
    }
    std::string cmd = "cmd.exe /C ping 127.0.0.1 -n 2 >nul & del /f /q \"" + std::string(selfPath) + "\"";
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

} // namespace

int main() {
    std::cout << "=== License Checker Install Kit ===" << std::endl;

    std::string srcDir = GetExecutableDirectory();
    if (srcDir.empty()) {
        std::cerr << "ERROR: could not determine the installer's own directory." << std::endl;
        return 1;
    }

    if (!CreateDirectoryA(kInstallDir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        std::cerr << "ERROR: failed to create " << kInstallDir << " (error " << GetLastError() << ")" << std::endl;
        std::cerr << "Make sure this installer is run as Administrator." << std::endl;
        return 1;
    }
    std::cout << "Install directory ready: " << kInstallDir << std::endl;

    // Reinstall case: if the service is already running, it holds
    // LicenseCheckerAgent.exe open and the copy below would fail with a
    // sharing violation. Stopping a service that isn't installed yet is a
    // normal, harmless no-op on a first install.
    StopServiceNow(kServiceName);
    Sleep(1500);

    if (!CopyRequiredFile(srcDir, kInstallDir, "LicenseCheckerAgent.exe", true)) {
        return 1;
    }
    if (!CopyRequiredFile(srcDir, kInstallDir, "agent_config.json", true)) {
        return 1;
    }
    CopyRequiredFile(srcDir, kInstallDir, "LicenseCheckerUI.exe", false);

    // Read server_url/api_key from the config just installed, so the
    // registration call uses the exact same server/department credential
    // the ongoing agent service will use.
    std::string serverUrl, apiKey;
    std::ifstream configFile(std::string(kInstallDir) + "\\agent_config.json", std::ios::binary);
    if (configFile.is_open()) {
        std::ostringstream buffer;
        buffer << configFile.rdbuf();
        std::string content = buffer.str();
        TryGetJsonStringAny(content, {"server_url", "serverUrl"}, serverUrl);
        TryGetJsonStringAny(content, {"api_key", "X-Api-Key", "apiKey"}, apiKey);
    }

    bool registered = RegisterMachine(serverUrl, apiKey);

    std::string agentExePath = std::string(kInstallDir) + "\\LicenseCheckerAgent.exe";
    if (!InstallService(kServiceName, kDisplayName, agentExePath)) {
        // InstallService() already printed the specific reason. On a
        // reinstall it removes and recreates the service registration
        // itself, so reaching here means that failed too (not just "it
        // already existed") - still attempt to start whatever is there.
        std::cout << "WARNING: service installation did not complete cleanly; attempting to start it anyway..." << std::endl;
    }

    bool serviceStarted = StartServiceNow(kServiceName);
    if (!serviceStarted) {
        std::cout << "WARNING: could not start the service automatically. It is installed and set to"
                  << std::endl
                  << "         auto-start on next boot; you can also start it now with: sc start "
                  << kServiceName << std::endl;
    }

    std::cout << std::endl << "=== Install complete ===" << std::endl;

    // Only open the UI and clean up the kit once everything is confirmed
    // working end-to-end (service running AND machine registered) - if
    // either failed, leave the kit files in place so the install can be
    // retried, and don't pop the UI up on an incomplete/broken install.
    if (registered && serviceStarted) {
        LaunchInstalledUI(kInstallDir);
        CleanupInstallKit(srcDir, kInstallDir);
    }

    return 0;
}
