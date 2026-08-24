# License Windows & Office - Cases Documentation

## Overview
Document tất cả các trường hợp license status của Windows và Office, cách detect, mapping giá trị, và display trên UI.

---

## Part 1: Windows License Cases

### License Status Enum (C++ Code)
```cpp
enum class LicenseStatus {
    Legitimate = 0,          // Windows là genuine và activated
    Cracked = 1,             // Windows sử dụng pirated/unauthorized license
    NotLicensed = 2,         // Windows chưa được license/activated
    UnableToDetermine = 3    // Không thể xác định status
};
```

### UI Display Mapping
| Enum Value | Display Text | Color | Icon |
|---|---|---|---|
| 0 (Legitimate) | **Licensed (Legitimate)** | Green ✓ | Check mark |
| 1 (Cracked) | **Licensed (Cracked)** | Red ✗ | Warning |
| 2 (NotLicensed) | **Not Licensed** | Orange ⚠ | Alert |
| 3 (UnableToDetermine) | **Unable to Determine** | Gray ? | Question |

---

## Part 2: Windows License Detection Cases

### Case 1: Legitimate License (Genuine & Activated)
**WMI SoftwareLicensingProduct.LicenseStatus = 1**

**Description:** Windows được license hợp lệ thông qua:
- Microsoft Direct purchase
- OEM pre-installed
- Volume License Agreement
- Legitimate activation key

**Code Detection:**
```cpp
if (licenseStatus == 1) {
    status = LicenseStatus::Legitimate;  // Enum = 0
    licenseInfo.licenseStatus = "Licensed";
}
```

**Characteristics:**
- ✓ Genuine license key
- ✓ Product activated
- ✓ License is current (not expired)
- ✓ No grace period

**Example Output:**
```json
{
  "licenseStatus": 0,
  "licenseStatusDetail": "Licensed",
  "name": "Windows(R), Pro edition",
  "description": "Windows(R) Operating System, OEM channel",
  "partialProductKey": "XXXXX",
  "productKeyChannel": "OEM:DM",
  "remainingRearmCount": 1001
}
```

**Display on UI:**
- Status: **Licensed (Legitimate)** ✓
- Color: Green
- KMS Status: Depending on configuration
- Rearm Count: 1001 (max)

**Real World Example:**
- Mua Win 10 Pro retail, activate với product key
- OEM Windows đã được pre-activated
- Volume License được manage bởi KMS

---

### Case 2: Not Licensed (Unlicensed)
**WMI SoftwareLicensingProduct.LicenseStatus = 0**

**Description:** Windows đã install nhưng chưa được activate:
- Windows không có license key
- License key chưa được activate
- License key bị revoke

**Code Detection:**
```cpp
if (licenseStatus == 0) {
    status = LicenseStatus::NotLicensed;  // Enum = 2
    licenseInfo.licenseStatus = "NotLicensed";
}
```

**Characteristics:**
- ✗ Không có license key hoặc key không hợp lệ
- ✗ Chưa activation
- ✗ System phát hành thông báo "Activate Windows"
- ✗ Watermark trên desktop
- ✗ Một số tính năng bị disable

**Example Output:**
```json
{
  "licenseStatus": 2,
  "licenseStatusDetail": "NotLicensed",
  "name": "Windows(R), Home edition",
  "description": "Windows(R) Operating System, RETAIL channel",
  "partialProductKey": "",
  "productKeyChannel": "",
  "remainingRearmCount": 1001
}
```

**Display on UI:**
- Status: **Not Licensed** ⚠
- Color: Orange/Red
- KMS Status: NotKMS
- Rearm Count: Available (1001)

**Real World Example:**
- Windows 10 chưa activate
- Nâng cấp OS nhưng product key không hợp lệ
- License key bị revoke/cancel

---

### Case 3: Grace Period (Initial Grace)
**WMI SoftwareLicensingProduct.LicenseStatus = 2-8**

**Description:** Windows đang ở grace period - chưa được activate nhưng có thời gian để activate:
- Initial grace period (30 ngày default)
- Grace period with notification
- Non-genuine grace period
- Out of tolerance grace period
- Extended grace period

**Code Detection:**
```cpp
if (licenseStatus >= 2 && licenseStatus <= 8) {
    status = LicenseStatus::NotLicensed;  // Enum = 2
    licenseInfo.licenseStatus = "GracePeriod";
}
```

**Grace Period Types:**

| WMI Value | Type | Duration | Notes |
|---|---|---|---|
| 2 | Initial grace period | 30 days | User chưa activate, còn thời gian |
| 3 | Initial grace with notification | 30 days | Thông báo activate |
| 4 | Non-genuine grace | 30 days | Windows detected non-genuine |
| 5 | Non-genuine with notification | 30 days | Non-genuine + alert |
| 6 | Out of tolerance grace | 30 days | Hardware changed, need reactivation |
| 7 | Out of tolerance with notification | 30 days | Hardware change + alert |
| 8 | Extended grace period | 180 days | Extended support period |

**Characteristics:**
- ⏱ Đang trong grace period
- ✓ System vẫn hoạt động bình thường
- ⚠ Có thông báo "Activate Windows"
- 📌 Watermark có thể hiển thị
- 🔔 Người dùng được nhắc nhở activate

**Example Output (Grace Period = 2):**
```json
{
  "licenseStatus": 2,
  "licenseStatusDetail": "GracePeriod",
  "name": "Windows(R), Enterprise edition",
  "description": "Windows(R) Operating System",
  "partialProductKey": "XXXXX",
  "productKeyChannel": "",
  "remainingRearmCount": 1001
}
```

**Display on UI:**
- Status: **Not Licensed** ⚠
- Secondary: "Grace Period - activate within 30 days"
- Color: Yellow/Orange
- Countdown: Show days remaining if available

**Real World Example:**
- Cài lại Windows, chưa activate
- Nâng cấp hardware, license cần reactivation
- Non-genuine Windows trong grace period

---

### Case 4: Cracked / Non-Genuine (WMI Value > 8)
**WMI SoftwareLicensingProduct.LicenseStatus > 8 (or other indicators)**

**Description:** Windows được detect là non-genuine/cracked:
- Sử dụng pirated key
- Sử dụng KMS emulator
- System file bị modify
- License bị tamper

**Code Detection:**
```cpp
if (licenseStatus > 8) {
    status = LicenseStatus::Cracked;  // Enum = 1
    licenseInfo.licenseStatus = "Cracked";
}
```

**Characteristics:**
- ✗ Non-genuine/pirated product key
- ✗ KMS emulator detected
- ✗ System file modification detected
- ✗ License validation failed
- 🚨 Microsoft watermark "Not Genuine"
- ⛔ Regular Windows Update may be limited

**Example Output:**
```json
{
  "licenseStatus": 1,
  "licenseStatusDetail": "Cracked",
  "name": "Windows(R), Pro edition",
  "description": "Windows(R) Operating System",
  "partialProductKey": "",
  "productKeyChannel": "",
  "remainingRearmCount": 0
}
```

**Display on UI:**
- Status: **Licensed (Cracked)** ✗
- Color: Red (Critical)
- Icon: Error/Warning
- Message: "Non-genuine license detected"

**Real World Example:**
- Using KMS patcher/emulator
- Pirated Windows installation
- System file tampering detected by SLUI

---

### Case 5: Unable to Determine (Detection Failed)
**Code Detection fails / WMI unavailable / Registry error**

**Description:** System tidak thể xác định license status do:
- WMI unavailable/disabled
- Registry corrupted
- Permission denied
- System error

**Code Detection:**
```cpp
if (wmiStatus == LicenseStatus::UnableToDetermine && 
    registryStatus == LicenseStatus::UnableToDetermine) {
    status = LicenseStatus::UnableToDetermine;  // Enum = 3
}
```

**Characteristics:**
- ? License status không xác định
- ⚠ Không thể detect via WMI
- ⚠ Không thể detect via Registry
- 🔧 Có thể cần troubleshooting

**Example Output:**
```json
{
  "licenseStatus": 3,
  "licenseStatusDetail": "Unable to Determine",
  "isError": true,
  "errorMessage": "WMI query failed and Registry inaccessible",
  "name": "",
  "description": ""
}
```

**Display on UI:**
- Status: **Unable to Determine** ?
- Color: Gray (Unknown)
- Message: "Could not determine license status. Check logs for details."
- Suggestion: "Run troubleshooter or check Event Viewer"

**Real World Example:**
- WMI service disabled
- Registry corrupted
- Insufficient permissions
- System in recovery mode

---

## Part 3: Windows License Notification Codes

### Notification Reason Codes (from slmgr.vbs)
Khi `slmgr.vbs /dlv` show "Notification" status, có specific reason code:

| Code | Hex | Meaning | Action |
|---|---|---|---|
| 0xC004F034 | License not properly configured | Windows edition not properly licensed | Need valid product key |
| 0xC004F050 | License not found | License file missing/corrupt | Repair/Reinstall Windows |
| 0xC004F038 | License expired | License period expired | Renew license |
| 0xC004F042 | License invalid for country | License not valid in this region | Contact Microsoft |

**Detection in Code:**
```cpp
// slmgr shows "Notification 0xC004F034" 
// but WMI LicenseStatus=2 (grace period)
// System correctly identifies as NotLicensed
```

---

## Part 4: KMS (Key Management Service) Cases

### Case 1: KMS Detected & Working
**KMSStatus = KMSDetected (Enum = 1)**

**Description:** Windows activated via KMS server:
- Corporate environment
- Volume License Agreement (VLA)
- KMS server available and responding
- Auto-renewal every 180 days

**Detection:**
```cpp
std::string windowsKmsServer = DetectWindowsKmsServer();
if (!windowsKmsServer.empty()) {
    result.SetKmsStatus(KMSStatus::KMSDetected);
    result.SetWindowsKmsServer(windowsKmsServer);
}
```

**Example Output:**
```json
{
  "kmsStatus": 1,
  "kmsServer": "kms.corp.local",
  "licenseStatus": 0
}
```

**Display on UI:**
- KMS Status: **KMS Detected** ✓
- KMS Server: `kms.corp.local`
- Activation: Auto-renewal enabled

**Registry Path:**
```
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\SoftwareProtectionPlatform\RPC
```

**Real World Example:**
- Corporate network
- Active Directory managed
- KMS server configured
- Auto-renewal working

---

### Case 2: No KMS (Local/Retail Activation)
**KMSStatus = NotKMS (Enum = 0)**

**Description:** Windows activated locally (not via KMS):
- Retail product key
- OEM pre-activated
- MAK (Multiple Activation Key) activated
- Online activation via Internet

**Detection:**
```cpp
std::string windowsKmsServer = DetectWindowsKmsServer();
if (windowsKmsServer.empty()) {
    result.SetKmsStatus(KMSStatus::NotKMS);
}
```

**Example Output:**
```json
{
  "kmsStatus": 0,
  "kmsServer": "",
  "licenseStatus": 0
}
```

**Display on UI:**
- KMS Status: **Not Using KMS**
- KMS Server: (empty/N/A)
- Activation: Direct activation

**Real World Example:**
- Home Edition
- Retail Pro License
- OEM Windows
- Direct product key activation

---

### Case 3: KMS Not Found / Server Unavailable
**KMSStatus = KMSNotFound (Enum = 2)**

**Description:** KMS configured nhưng server unavailable:
- KMS server down
- Network unreachable
- Wrong KMS address configured
- Grace period active (can work for 30 days)

**Detection:**
```cpp
// Try to reach KMS server - if fails:
result.SetKmsStatus(KMSStatus::KMSNotFound);
```

**Example Output:**
```json
{
  "kmsStatus": 2,
  "kmsServer": "kms.corp.local (unreachable)",
  "licenseStatus": 2
}
```

**Display on UI:**
- KMS Status: **KMS Not Found / Unavailable** ⚠
- KMS Server: `kms.corp.local (unreachable)`
- Warning: "KMS server not responding - grace period active"

**Real World Example:**
- KMS server down for maintenance
- Network disconnected
- Firewall blocking KMS port
- Laptop outside corporate network

---

## Part 5: Office License Cases

### Office License Status Values
```cpp
enum class OfficeLicenseStatus {
    OfficeLicensed = 0,      // Office activated & licensed
    OfficeNotLicensed = 1,   // Office not licensed
    OfficeGracePeriod = 2,   // Office in grace period
    OfficeUnableToDetermine = 3  // Cannot determine
};
```

### Case 1: Office Licensed & Activated
**Status = 0 (Licensed)**

**Description:** Microsoft Office is properly activated:
- Office 365 / Microsoft 365 subscription active
- Office 2016/2019/2021 with valid license key
- Perpetual license activated
- Volume License Agreement active

**Detection Logic:**
```cpp
LicenseStatus RegistryDetector::DetectOfficeLicenseStatus() {
    // Query Office registry for license status
    // If Office detected and licensed: return Licensed
    // If Office detected but not licensed: return NotLicensed
}
```

**Registry Paths (Office):**
```
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Office\ClickToRun\Configuration
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Office\16.0\Common\OEM
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Office\15.0\Common\OEM
```

**Example Output:**
```json
{
  "office": {
    "licenseStatus": 0,
    "licenseName": "Microsoft 365 Apps for enterprise",
    "licenseDescription": "Microsoft 365 subscription",
    "kmsServer": "kms.corp.local",
    "productId": "365",
    "remainingGrace": "N/A"
  }
}
```

**Display on UI:**
- Status: **Office Licensed** ✓
- License Name: `Microsoft 365 Apps for enterprise`
- Activation: Subscription active or MAK/KMS activated
- KMS: If using KMS server

---

### Case 2: Office Not Licensed
**Status = 1 (Not Licensed)**

**Description:** Office not activated or no valid license:
- Office installed but not activated
- Trial period expired
- Subscription expired
- No license key

**Example Output:**
```json
{
  "office": {
    "licenseStatus": 1,
    "licenseName": "Office (Not Licensed)",
    "licenseDescription": "Microsoft Office - Not Activated",
    "kmsServer": "",
    "remainingGrace": "0 days"
  }
}
```

**Display on UI:**
- Status: **Office Not Licensed** ⚠
- License Name: `Not Licensed`
- Message: "Office requires activation"
- Action: "Enter product key or activate subscription"

---

### Case 3: Office Grace Period
**Status = 2 (Grace Period)**

**Description:** Office in grace period (usually 30 days):
- Trial period active
- License expired but within grace
- Subscription lapsed but grace active

**Example Output:**
```json
{
  "office": {
    "licenseStatus": 2,
    "licenseName": "Office (Grace Period)",
    "licenseDescription": "Trial or grace period active",
    "remainingGrace": "25 days"
  }
}
```

**Display on UI:**
- Status: **Office Grace Period** ⏱
- Remaining: `25 days remaining`
- Warning: "Activate Office before grace period expires"

---

### Case 4: Office Not Detected
**Status = 3 (Unable to Determine / Not Installed)**

**Description:** Office not installed or cannot detect:
- Microsoft Office not installed
- Cannot access Office registry
- Office in different location
- Portable/standalone version

**Example Output:**
```json
{
  "office": {
    "licenseStatus": 3,
    "licenseName": "Not Detected",
    "licenseDescription": "Microsoft Office not found on this system"
  }
}
```

**Display on UI:**
- Status: **Office Not Detected** -
- Message: "Microsoft Office is not installed on this system"

---

## Part 6: Windows Version & Edition Cases

### Windows Versions (Enum)
```cpp
constexpr int WINDOWS_VERSION_7 = 7;
constexpr int WINDOWS_VERSION_8 = 8;
constexpr int WINDOWS_VERSION_81 = 81;
constexpr int WINDOWS_VERSION_10 = 10;
constexpr int WINDOWS_VERSION_11 = 11;
constexpr int WINDOWS_SERVER_2008 = 2008;
constexpr int WINDOWS_SERVER_2012 = 2012;
constexpr int WINDOWS_SERVER_2016 = 2016;
constexpr int WINDOWS_SERVER_2019 = 2019;
constexpr int WINDOWS_SERVER_2022 = 2022;
```

### Windows Editions
| Edition | Code | Usage |
|---|---|---|
| Home | HOME | Consumer, Home users |
| Pro | PROFESSIONAL | Business, Power users |
| Enterprise | ENTERPRISE | Large organizations, VLA |
| Education | EDUCATION | Schools, educational institutions |
| Server 2008 R2 | SERVER | Data centers |
| Server 2012 R2 | SERVER | Data centers |
| Server 2016 | SERVER | Cloud, on-premise |
| Server 2019 | SERVER | Modern workloads |
| Server 2022 | SERVER | Latest workloads |

### Detection Code:
```cpp
void RegistryDetector::DetectWindowsVersionAndEdition(int& outVersion, std::string& outEdition) {
    outVersion = GetWindowsVersion();  // Returns: 7, 8, 10, 11
    outEdition = GetWindowsEdition();  // Returns: "Pro", "Home", "Enterprise"
}
```

---

## Part 7: Combined License Scenarios

### Scenario 1: Enterprise - Licensed via KMS
```json
{
  "windows": {
    "version": 10,
    "edition": "Enterprise",
    "licenseStatus": 0,
    "kmsStatus": 1,
    "kmsServer": "kms.corp.local",
    "name": "Windows(R), Enterprise edition",
    "description": "Windows(R) Operating System",
    "partialProductKey": "XXXXX"
  },
  "office": {
    "licenseStatus": 0,
    "licenseName": "Microsoft 365 Apps for enterprise",
    "kmsServer": "kms.corp.local"
  }
}
```
**Display:**
- Windows: **Licensed (Legitimate)** ✓ via KMS
- Office: **Licensed** ✓ via KMS
- Rearm: Auto-renewal every 180 days

---

### Scenario 2: Home User - Retail License
```json
{
  "windows": {
    "version": 11,
    "edition": "Home",
    "licenseStatus": 0,
    "kmsStatus": 0,
    "kmsServer": "",
    "name": "Windows(R), Home edition",
    "description": "Windows(R) Operating System, RETAIL channel",
    "partialProductKey": "ABC12"
  },
  "office": {
    "licenseStatus": 0,
    "licenseName": "Microsoft 365 Personal",
    "remainingGrace": "N/A"
  }
}
```
**Display:**
- Windows: **Licensed (Legitimate)** ✓ (Retail)
- Office: **Licensed** ✓ (Subscription)
- Status: Fully compliant

---

### Scenario 3: Unlicensed System
```json
{
  "windows": {
    "version": 10,
    "edition": "Pro",
    "licenseStatus": 2,
    "kmsStatus": 0,
    "name": "Windows(R), Pro edition",
    "partialProductKey": ""
  },
  "office": {
    "licenseStatus": 1,
    "licenseName": "Office (Not Licensed)"
  }
}
```
**Display:**
- Windows: **Not Licensed** ⚠
- Office: **Office Not Licensed** ⚠
- Status: Both need activation

---

### Scenario 4: Cracked System
```json
{
  "windows": {
    "version": 10,
    "edition": "Pro",
    "licenseStatus": 1,
    "name": "Windows(R), Pro edition",
    "partialProductKey": ""
  },
  "office": {
    "licenseStatus": 1
  }
}
```
**Display:**
- Windows: **Licensed (Cracked)** ✗
- Office: **Office Not Licensed** ⚠
- Status: Non-genuine, needs legitimate license

---

## Part 8: License Rearm Functionality

### What is Rearm?
- Allows reinstalling OS on same hardware
- Resets grace period counter
- Limited number of rearms per lifetime

### Remaining Rearm Count
```json
{
  "remainingRearmCount": 1001,
  "explanation": "Max value (1001) = unlimited rearms available"
}
```

### When Rearm Count Decreases:
- Running `slmgr /rearm` command
- Major hardware changes
- System recovery/repair

### Critical Thresholds:
| Count | Status | Action |
|---|---|---|
| 1001 | Max / Unlimited | No action needed |
| 10-100 | Moderate | Monitor rearm usage |
| 1-5 | Critical | Few rearms left |
| 0 | Depleted | Cannot rearm anymore |

---

## Part 9: Error Handling in Code

### Error Cases in Detection

**Case 1: WMI Query Error**
```cpp
// When: WMI service disabled or unavailable
// Code returns: LicenseStatus::UnableToDetermine (Enum = 3)
// Fallback: Try Registry detection
```

**Case 2: Registry Error**
```cpp
// When: Registry key not found or corrupted
// Code returns: LicenseStatus::UnableToDetermine (Enum = 3)
// Action: Log error, continue with available data
```

**Case 3: Permission Denied**
```cpp
// When: No admin privileges for detection
// Code returns: LicenseStatus::UnableToDetermine (Enum = 3)
// Message: "Insufficient permissions for license detection"
```

**Case 4: WMI Field Empty**
```cpp
// When: WMI query succeeds but specific fields empty
// Code: Set fields to "" (empty string)
// Status: Still return license status from LicenseStatus field
// Example: No Name/Description but status=1 → still return Legitimate
```

---

## Part 10: Summary Reference

### License Status Enum Mapping
```
0 = Legitimate    → "Licensed (Legitimate)" ✓ Green
1 = Cracked       → "Licensed (Cracked)"    ✗ Red  
2 = NotLicensed   → "Not Licensed"          ⚠ Orange
3 = UnableToDet.  → "Unable to Determine"   ? Gray
```

### KMS Status Enum Mapping
```
0 = NotKMS        → "Not Using KMS"        (Retail/Direct)
1 = KMSDetected   → "KMS Detected"         (KMS Server active)
2 = KMSNotFound   → "KMS Not Found"        (KMS configured but unavailable)
3 = Error         → "Error"                (Detection error)
```

### Detection Priority (Fallback)
```
1. WMI SoftwareLicensingProduct query
   ↓ (if fails)
2. Registry HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\SoftwareProtectionPlatform
   ↓ (if fails)
3. Return: UnableToDetermine
```

### Display Priority (UI)
```
1. Show License Status (Legitimate/Cracked/NotLicensed/Unable)
2. Show Windows Edition & Name
3. Show KMS Status (if applicable)
4. Show Remaining Rearm Count
5. Show Last Detected Timestamp
```

---

## Troubleshooting Guide

### Issue: License Status shows "Unable to Determine"
**Solution:**
1. Check if WMI service is running: `sc query winmgmt`
2. Check registry access: Verify admin privileges
3. Check logs for specific error
4. Run slmgr.vbs /dlv manually to verify

### Issue: KMS Server not detected despite configured
**Solution:**
1. Verify KMS address in registry
2. Check network connectivity to KMS server
3. Verify firewall allows port 1688
4. Check Event Viewer for KMS errors

### Issue: WMI fields showing empty in UI
**Solution:**
1. Verify WMI query extraction code (Test-1.4)
2. Check if WindowsLicenseInfo populated correctly
3. Verify SetWindowsLicenseInfo() called
4. Check JSON response contains fields

### Issue: License shows as "Not Licensed" but Windows appears activated
**Solution:**
1. Check WMI LicenseStatus value (should be 0 or 2-8 for NotLicensed)
2. Check if in grace period (codes 2-8)
3. Run slmgr.vbs /dlv for verification
4. Notification code 0xC004F034 = not genuine

---

## Future Enhancements

- [ ] Add Office license detection via COM/WMI
- [ ] Add Support for Windows Server editions
- [ ] Add License expiration countdown
- [ ] Add Rearm count warning thresholds
- [ ] Add License compliance reporting
- [ ] Add Automated remediation suggestions
- [ ] Add Microsoft 365 subscription status
- [ ] Add License validation via online API

