# Agent-Server Integration Guide

## Overview

Agent (C++ trên Windows) giao tiếp với Server (Python FastAPI) qua HTTP(S) sử dụng JSON format.

---

## 1. Protocol & Authentication

### Giao Thức
- **Protocol:** HTTP (development) / HTTPS (production)
- **Method:** POST
- **Content-Type:** application/json
- **Format:** JSON

### Authentication
```
Header: X-API-Key: {api-key-from-dashboard}
```

API Key được sinh từ dashboard, định danh agent/department.

---

## 2. Endpoint

### Server Endpoint
```
POST /api/agent/report-license
```

### Full URL
```
http://server-ip:8000/api/agent/report-license
(hoặc https:// cho production)
```

### Response
```
HTTP 200 OK
{
  "status": "received",
  "machine_id": 42,
  "timestamp": "2026-08-06T19:00:00Z"
}
```

---

## 3. Machine Identification

### Cách định danh máy

Agent cần gửi các thông tin sau để định danh máy:

```json
{
  "hostname": "DESKTOP-ABC123",        // Tên máy Windows
  "department": "Finance",             // Phòng ban
  "os_version": 10,                    // 7, 8, 10, 11, hoặc Server version
  "os_edition": "Pro"                  // Pro, Enterprise, Home, Server 2016, etc.
}
```

### Cách lấy thông tin máy (C++ code)

```cpp
// Lấy hostname
char hostname[MAX_COMPUTERNAME_LENGTH + 1];
DWORD size = sizeof(hostname);
GetComputerNameA(hostname, &size);

// Lấy Windows version
DWORD version = GetWindowsVersion();  // 10, 11, etc.
std::string edition = GetWindowsEdition();  // "Pro", "Enterprise", etc.

// Lấy machine GUID (optional, để tracking)
WCHAR guid[256];
LPGUID guid_ptr = GetMachineGUID();
```

### Server Side: Xử lý định danh

Server tự động:
1. Kiểm tra nếu máy đã tồn tại (dựa vào hostname)
2. Nếu chưa → tạo record mới
3. Nếu có → cập nhật thông tin
4. Ghi vào lịch sử (license_history)

---

## 4. License Detection Data Format

### JSON Structure gửi lên

```json
{
  "hostname": "DESKTOP-ABC123",
  "department": "Finance",
  "os_version": 10,
  "os_edition": "Pro",
  
  "licenseStatus": "Legitimate",
  "windowsEdition": "Pro",
  "officeStatus": "Legitimate",
  "officeEdition": "Microsoft 365",
  
  "kmsStatus": "KMSDetected",
  "windowsKmsServer": "kms.corp.local",
  "officeKmsServer": "kms.corp.local",
  
  "isError": false,
  "errorMessage": null,
  
  "timestamp": "2026-08-06T19:00:00Z"
}
```

### Field Descriptions

#### Định danh máy
| Field | Type | Giải thích |
|-------|------|-----------|
| hostname | string | Tên máy Windows (từ GetComputerName) |
| department | string | Phòng ban/department (có thể hardcode hoặc config) |
| os_version | integer | 7, 8, 10, 11, hoặc 2008, 2012, 2016, 2019, 2022 |
| os_edition | string | Pro, Enterprise, Home, Server 2016, etc. |

#### Windows License
| Field | Type | Giải thích |
|-------|------|-----------|
| licenseStatus | enum | Legitimate \| Cracked \| NotLicensed \| UnableToDetermine |
| windowsEdition | string | Windows edition (Pro, Enterprise, Home) |
| kmsStatus | enum | NotKMS \| KMSDetected \| KMSNotFound \| Error |
| windowsKmsServer | string | Hostname/IP của KMS server (nếu KMS được dùng) |

#### Office License
| Field | Type | Giải thích |
|-------|------|-----------|
| officeStatus | enum | Legitimate \| Cracked \| NotLicensed \| UnableToDetermine |
| officeEdition | string | Microsoft 365, Office 2019, Office 2021, etc. |
| officeKmsServer | string | Hostname/IP của Office KMS server |

#### Error & Metadata
| Field | Type | Giải thích |
|-------|------|-----------|
| isError | boolean | true nếu detection lỗi |
| errorMessage | string | Chi tiết lỗi (nếu có) |
| timestamp | string | ISO 8601 format: "2026-08-06T19:00:00Z" |

---

## 5. Complete Agent Report Example

### Example 1: Legitimate License (KMS)
```json
POST /api/agent/report-license
X-API-Key: agent-key-12345
Content-Type: application/json

{
  "hostname": "WORKSTATION-001",
  "department": "Finance",
  "os_version": 10,
  "os_edition": "Pro",
  
  "licenseStatus": "Legitimate",
  "windowsEdition": "Pro",
  "officeStatus": "Legitimate",
  "officeEdition": "Microsoft 365",
  
  "kmsStatus": "KMSDetected",
  "windowsKmsServer": "kms.corp.local",
  "officeKmsServer": "kms.corp.local",
  
  "isError": false,
  "errorMessage": null,
  
  "timestamp": "2026-08-06T19:00:00Z"
}
```

**Server Response:**
```json
{
  "status": "received",
  "machine_id": 1,
  "timestamp": "2026-08-06T19:00:00Z"
}
```

### Example 2: Cracked License
```json
POST /api/agent/report-license
X-API-Key: agent-key-12345
Content-Type: application/json

{
  "hostname": "DESKTOP-XYZ789",
  "department": "Engineering",
  "os_version": 11,
  "os_edition": "Home",
  
  "licenseStatus": "Cracked",
  "windowsEdition": "Home",
  "officeStatus": "Cracked",
  "officeEdition": "Unknown",
  
  "kmsStatus": "NotKMS",
  "windowsKmsServer": null,
  "officeKmsServer": null,
  
  "isError": false,
  "errorMessage": null,
  
  "timestamp": "2026-08-06T19:00:00Z"
}
```

### Example 3: Grace Period (Not Licensed)
```json
POST /api/agent/report-license
X-API-Key: agent-key-12345
Content-Type: application/json

{
  "hostname": "LAPTOP-NEW-001",
  "department": "HR",
  "os_version": 10,
  "os_edition": "Pro",
  
  "licenseStatus": "NotLicensed",
  "windowsEdition": "Pro",
  "officeStatus": null,
  "officeEdition": null,
  
  "kmsStatus": "NotKMS",
  "windowsKmsServer": null,
  "officeKmsServer": null,
  
  "isError": false,
  "errorMessage": "Windows in grace period (30 days unactivated)",
  
  "timestamp": "2026-08-06T19:00:00Z"
}
```

### Example 4: Detection Error
```json
POST /api/agent/report-license
X-API-Key: agent-key-12345
Content-Type: application/json

{
  "hostname": "DESKTOP-ERROR-001",
  "department": "IT",
  "os_version": 10,
  "os_edition": "Pro",
  
  "licenseStatus": "UnableToDetermine",
  "windowsEdition": null,
  "officeStatus": null,
  "officeEdition": null,
  
  "kmsStatus": "Error",
  "windowsKmsServer": null,
  "officeKmsServer": null,
  
  "isError": true,
  "errorMessage": "SL APIs unavailable - permission denied",
  
  "timestamp": "2026-08-06T19:00:00Z"
}
```

---

## 6. HTTP Request Implementation (C++)

### Using cURL (Easiest)

```cpp
#include <curl/curl.h>
#include <string>
#include <json.hpp>  // nlohmann/json

void SendLicenseReport(const std::string& serverUrl, 
                       const std::string& apiKey,
                       const nlohmann::json& licenseData) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    std::string fullUrl = serverUrl + "/api/agent/report-license";
    std::string jsonData = licenseData.dump();
    std::string apiKeyHeader = "X-API-Key: " + apiKey;

    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, apiKeyHeader.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);  // For HTTPS testing
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", 
                curl_easy_strerror(res));
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

// Usage
int main() {
    nlohmann::json report = {
        {"hostname", "DESKTOP-ABC123"},
        {"department", "Finance"},
        {"os_version", 10},
        {"os_edition", "Pro"},
        {"licenseStatus", "Legitimate"},
        {"windowsEdition", "Pro"},
        {"kmsStatus", "KMSDetected"},
        {"windowsKmsServer", "kms.corp.local"},
        {"timestamp", "2026-08-06T19:00:00Z"}
    };
    
    SendLicenseReport("http://localhost:8000", 
                     "agent-api-key-12345",
                     report);
}
```

### Using WinHTTP (Windows Native)

```cpp
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

void SendLicenseReportWinHTTP(const std::wstring& serverHost,
                               const std::string& jsonData,
                               const std::string& apiKey) {
    HINTERNET hSession = WinHttpOpen(L"License-Checker-Agent/1.0",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);

    HINTERNET hConnect = WinHttpConnect(hSession, serverHost.c_str(),
                                         INTERNET_DEFAULT_HTTP_PORT, 0);

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
                                             L"/api/agent/report-license",
                                             NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, 0);

    // Set headers
    std::wstring apiKeyHeader = L"X-API-Key: " + 
                                std::wstring(apiKey.begin(), apiKey.end());
    WinHttpAddRequestHeaders(hRequest, apiKeyHeader.c_str(),
                            (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest,
                            L"Content-Type: application/json",
                            (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

    // Send request
    WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS,
                       0, (LPVOID)jsonData.c_str(),
                       jsonData.length(), jsonData.length(), 0);

    // Wait for response
    WinHttpReceiveResponse(hRequest, NULL);

    // Clean up
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
}
```

---

## 7. Agent Configuration

### Configuration File (JSON)

**File:** `C:\Program Files\LicenseChecker\config.json`

```json
{
  "server": {
    "url": "http://your-server-ip:8000",
    "api_key": "generated-from-dashboard",
    "timeout_seconds": 30,
    "verify_ssl": false
  },
  "agent": {
    "report_interval_minutes": 5,
    "machine_info": {
      "hostname": "auto",
      "department": "Default"
    },
    "retry": {
      "max_attempts": 3,
      "delay_seconds": 60
    }
  },
  "logging": {
    "level": "INFO",
    "file": "license-detection.log"
  }
}
```

### Configuration Reading (C++)

```cpp
#include <fstream>
#include <json.hpp>

class AgentConfig {
public:
    std::string serverUrl;
    std::string apiKey;
    int reportIntervalMinutes;
    std::string department;
    int maxRetries;

    bool LoadFromFile(const std::string& configPath) {
        std::ifstream configFile(configPath);
        if (!configFile.is_open()) {
            std::cerr << "Failed to open config file: " << configPath << std::endl;
            return false;
        }

        try {
            nlohmann::json config;
            configFile >> config;

            serverUrl = config["server"]["url"].get<std::string>();
            apiKey = config["server"]["api_key"].get<std::string>();
            reportIntervalMinutes = config["agent"]["report_interval_minutes"].get<int>();
            department = config["agent"]["machine_info"]["department"].get<std::string>();
            maxRetries = config["agent"]["retry"]["max_attempts"].get<int>();

            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error parsing config: " << e.what() << std::endl;
            return false;
        }
    }
};
```

---

## 8. Workflow: Detection → Report → Server

```
Agent (Windows Machine)
│
├─ Every 5 minutes (configurable)
│
├─ 1. DETECT
│   ├─ Tier 1: SL API
│   ├─ Tier 2: WMI (if Tier 1 fails)
│   └─ Tier 3: Registry (if Tier 2 fails)
│   └─ Result: LicenseResult object
│
├─ 2. PREPARE
│   ├─ Get hostname: GetComputerName()
│   ├─ Get OS version: GetWindowsVersion()
│   ├─ Get KMS server: Query registry
│   ├─ Create JSON: LicenseResult::ToJSON()
│   └─ Load config: Read server URL & API key
│
├─ 3. SEND
│   ├─ POST to: /api/agent/report-license
│   ├─ Headers: X-API-Key, Content-Type
│   ├─ Body: JSON license data
│   ├─ Retry: Max 3 attempts with delay
│   └─ Log: Success or error
│
└─ Server (Python FastAPI)
   │
   ├─ 4. RECEIVE
   │   ├─ Validate API key
   │   ├─ Parse JSON
   │   └─ Verify required fields
   │
   ├─ 5. PROCESS
   │   ├─ Find/create machine record
   │   ├─ Archive old license to history
   │   ├─ Update license record
   │   └─ Update last_check_in
   │
   └─ 6. RESPOND
       ├─ Return HTTP 200
       ├─ Return machine_id
       └─ Return timestamp
```

---

## 9. Error Handling & Retry Logic

### Agent-side Error Handling

```cpp
class LicenseReporter {
private:
    int retryCount = 0;
    const int MAX_RETRIES = 3;
    const int RETRY_DELAY_SECONDS = 60;

public:
    bool ReportLicense(const LicenseResult& result) {
        nlohmann::json jsonData = {
            {"hostname", GetComputerName()},
            {"licenseStatus", LicenseStatusToString(result.status)},
            // ... other fields
        };

        for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
            try {
                bool success = SendHTTPRequest(jsonData);
                if (success) {
                    LogMessage("License report sent successfully");
                    return true;
                }
            } catch (const std::exception& e) {
                LogError("Send attempt " + std::to_string(attempt + 1) + 
                        " failed: " + std::string(e.what()));
                
                if (attempt < MAX_RETRIES - 1) {
                    Sleep(RETRY_DELAY_SECONDS * 1000);
                }
            }
        }

        LogError("Failed to send license report after " + 
                std::to_string(MAX_RETRIES) + " attempts");
        return false;
    }
};
```

### Server-side Response Codes

| Code | Status | Meaning |
|------|--------|---------|
| 200 | OK | License report received and processed |
| 400 | Bad Request | Invalid JSON or missing required fields |
| 401 | Unauthorized | API key missing or invalid |
| 500 | Server Error | Database or processing error |

---

## 10. Gửi dữ liệu qua HTTPS (Production)

### Certificate Setup

```cpp
// Disable SSL verification (testing only - SECURITY RISK)
curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

// Proper certificate validation (production)
curl_easy_setopt(curl, CURLOPT_CAINFO, "ca-bundle.crt");
curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
```

---

## 11. Testing Connection

### Test with curl (từ agent machine)

```bash
# Test connection
curl -v http://server-ip:8000/api/health

# Test sending license report
curl -X POST http://server-ip:8000/api/agent/report-license \
  -H "X-API-Key: agent-key-12345" \
  -H "Content-Type: application/json" \
  -d '{
    "hostname": "TEST-MACHINE",
    "department": "IT",
    "os_version": 10,
    "os_edition": "Pro",
    "licenseStatus": "Legitimate",
    "kmsStatus": "KMSDetected",
    "windowsKmsServer": "kms.corp.local",
    "timestamp": "2026-08-06T19:00:00Z"
  }'
```

---

## 12. Dashboard Verification

### Sau khi agent gửi report

1. **Mở Dashboard:** http://localhost:8000/dashboard
2. **Login:** admin / admin123
3. **Machines tab:** Xem máy vừa report
4. **Violations tab:** Nếu license bị crack
5. **Audit log:** Xem tất cả activities

---

## 13. Quick Checklist

- [ ] Server đang chạy (port 8000)
- [ ] API key sinh từ dashboard
- [ ] Agent config file tạo và đúng
- [ ] Hostname có thể lấy được (GetComputerName)
- [ ] OS version detection hoạt động
- [ ] JSON formatting đúng
- [ ] Timestamp ISO 8601 format
- [ ] Network connectivity giữa agent và server
- [ ] Firewall cho phép port 8000
- [ ] Logging đang record successes/failures
- [ ] Dashboard hiển thị incoming reports
- [ ] Violations được flag chính xác

---

## 14. Troubleshooting

### Server không nhận được report

```
Kiểm tra:
1. Server chạy? → curl http://localhost:8000/api/health
2. API key đúng? → Check settings tab trên dashboard
3. Network? → ping server-ip từ agent machine
4. Firewall? → Allow port 8000
5. JSON format? → Validate với curl trước
```

### Agent lỗi khi gửi

```
Kiểm tra:
1. Config file tồn tại? → C:\Program Files\LicenseChecker\config.json
2. Server URL đúng? → URL không có trailing slash
3. API key format? → Không có spaces
4. Retry logic? → Check logs cho attempt count
5. Certificate? → HTTPS cert hợp lệ?
```

### Dashboard không hiển thị report

```
Kiểm tra:
1. Report gửi thành công? → Check agent logs
2. Server logs? → Check FastAPI output
3. Database? → Machines table có record?
4. Refresh dashboard? → F5 hoặc Ctrl+R
5. Browser cache? → Ctrl+Shift+Del clear cache
```

---

## Summary

**Agent → Server Communication:**

```
HTTP POST → /api/agent/report-license
Header: X-API-Key: {key}
Body: JSON {
  hostname, os_version, licenseStatus,
  kmsStatus, timestamp, ...
}
↓
Server: Validate key → Parse JSON → 
  Find/Create machine → Update license → Log audit
↓
Response: 200 OK + machine_id
↓
Dashboard: Display immediately
```

**Chu kỳ:** Mỗi 5 phút agent gửi report lên, server cập nhật database, dashboard hiển thị real-time.

