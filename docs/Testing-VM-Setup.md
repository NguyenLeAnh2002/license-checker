# Real VM Setup Guide - Windows License Testing

## Overview
Hướng dẫn chi tiết thiết lập các Windows VMs với các trạng thái license khác nhau để test License Checker Agent trên môi trường thực tế.

---

## System Requirements

### Host Machine
- Virtualization support (Hyper-V, VirtualBox, VMware)
- Minimum 32 GB RAM (4 VMs × 4GB mỗi cái)
- 200 GB disk space
- Windows 8+ hoặc Linux

### VM Specs (mỗi cái)
- OS: Windows 8 / 10 / 11
- RAM: 4 GB minimum
- Disk: 50 GB
- Network: Bridged hoặc NAT (để communicate với host)

### Tools Cần
- Virtualization software (Hyper-V / VirtualBox / VMware)
- Windows ISO file
- `slmgr.vbs` (có sẵn trên Windows)
- PowerShell (Windows 7+)
- License Checker Agent executable

---

## Part 1: VM Setup Checklist

### Pre-Installation
- [ ] Download Windows ISO
- [ ] Allocate hardware resources (CPU, RAM, Disk)
- [ ] Enable network connectivity
- [ ] Plan hostnames (VM-Legitimate, VM-NotLicensed, VM-GracePeriod, VM-Cracked)

### Post-Installation (mỗi VM)
- [ ] Install Windows + all updates
- [ ] Enable RDP (Remote Desktop) cho remote testing
- [ ] Disable UAC (User Account Control) để dễ test
- [ ] Enable PowerShell execution policy
- [ ] Disable Windows Defender (tuỳ chọn, để test không bị chậm)

---

## Part 2: VM 1 - Legitimate License (Genuine & Activated)

### Goal
Windows được activate với **genuine product key** hoặc **OEM pre-activation**.

### Setup Steps

#### Option A: Activate với Retail Product Key
```powershell
# 1. Kiểm tra license status hiện tại
slmgr.vbs /dlv

# 2. Install/change product key (nếu chưa có)
slmgr.vbs /ipk XXXXX-XXXXX-XXXXX-XXXXX-XXXXX

# 3. Activate online
slmgr.vbs /ato

# 4. Verify activation thành công
slmgr.vbs /dlv
```

#### Option B: Dùng KMS Activation (Enterprise-like)
```powershell
# 1. Change để Enterprise/Professional edition
# Settings > System > Activation > Change product key
# Chọn Enterprise hoặc Professional

# 2. Setup KMS server (hoặc dùng corporate KMS)
# Nếu không có corporate KMS, có thể dùng "công cộng" test KMS
slmgr.vbs /skms kms.digiboy.ir  # hoặc KMS server khác

# 3. Activate via KMS
slmgr.vbs /ato

# 4. Verify
slmgr.vbs /dlv
```

### Verification Checklist

```powershell
# Chạy đầy đủ diagnostic
slmgr.vbs /dlv
```

**Expected Output (Legitimate):**
```
License Status: Initial grace period
Grace Period Remaining: Not Applicable
→ HOẶC
License Status: Licensed
Grace Period Remaining: Not Applicable
```

**Check WMI directly:**
```powershell
Get-WmiObject -Namespace "root\cimv2" -Class SoftwareLicensingProduct | 
  Where-Object { $_.PartialProductKey } | 
  Select-Object Name, Description, LicenseStatus, @{N="Status";E={
    switch($_.LicenseStatus){
      1 {"Licensed"}
      0 {"Not Licensed"}
      default {"Other ($_)"}
    }
  }}
```

**Expected LicenseStatus = 1 (Licensed)**

### Characteristics to Verify
- ✓ No "Activate Windows" watermark
- ✓ License Status shows "Licensed"
- ✓ Remaining grace period = N/A
- ✓ Rearm count = 1001 (max)
- ✓ Product Key Channel = OEM:DM hoặc VOLUME:KMS

---

## Part 3: VM 2 - Not Licensed (Unlicensed)

### Goal
Windows được install nhưng **không activate** - trạng thái mặc định sau install.

### Setup Steps

#### Option A: Fresh Install (Easiest)
```powershell
# 1. Sau khi install Windows xong, KHÔNG activate
# 2. Skip activation step trong setup
# 3. Bỏ qua product key entry

# 4. Verify status (sẽ là "Not Licensed")
slmgr.vbs /dlv
```

#### Option B: Deactivate từ Legitimate
```powershell
# 1. Nếu VM đã activate sẵn, uninstall license
slmgr.vbs /upk

# 2. Clear license từ registry
slmgr.vbs /rearm  # Nếu được phép (rearm count > 0)

# 3. Verify
slmgr.vbs /dlv
```

### Verification Checklist

```powershell
slmgr.vbs /dlv
```

**Expected Output (Not Licensed):**
```
License Status: Initial grace period
Grace Period Remaining: N days
→ HOẶC
License Status: Notifications (Activate Windows)
```

**Check WMI:**
```powershell
Get-WmiObject -Namespace "root\cimv2" -Class SoftwareLicensingProduct | 
  Where-Object { $_.PartialProductKey } | 
  Select-Object Name, LicenseStatus
```

**Expected LicenseStatus = 0 (Not Licensed)**

### Characteristics to Verify
- ✗ "Activate Windows" watermark visible (bottom-right)
- ✗ License Status shows "Not Licensed" hoặc "Initial Grace Period"
- ✓ Rearm count still available (1001)
- ✗ PartialProductKey = empty
- ✓ System vẫn hoạt động bình thường (chỉ không được update)

---

## Part 4: VM 3 - Grace Period (Initial Grace / Out of Tolerance)

### Goal
Windows ở trong **grace period** - chưa activate nhưng có thời gian để activate.

### Setup Steps

#### Option A: Trigger Out-of-Tolerance Grace
```powershell
# 1. Từ VM Legitimate (LicenseStatus=1)
# 2. Modify hardware (thêm CPU, RAM, disk storage)
# 3. Windows sẽ detect "hardware change" → Out of tolerance grace

# 4. Run WMI check lại
slmgr.vbs /dlv

# Expected: License Status = 6 hoặc 7 (Out of tolerance grace)
```

#### Option B: Simulate Grace Period via Registry (Advanced)
```powershell
# ⚠️ Cẩn thận - modify registry trực tiếp
# 1. Open Registry Editor
regedit

# 2. Tìm tới: HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\SoftwareProtectionPlatform

# 3. Tìm giá trị: "BackupProductKeyDefault" hoặc "ProductKeyState"
# 4. Dùng "slmgr.vbs /rearm" để reset về initial grace period

slmgr.vbs /rearm
# Expected output: "Successful"

# 5. Verify
slmgr.vbs /dlv
```

#### Option C: Manual License Validation Service Manipulation
```powershell
# 1. Stop Software Protection Platform service
Stop-Service -Name "sppsvc" -Force

# 2. Backup key registry
reg export "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\SoftwareProtectionPlatform" backup.reg

# 3. Modify trạng thái tùy ý (cần knowledge sâu hơn)

# 4. Restart service
Start-Service -Name "sppsvc"

# 5. Verify
slmgr.vbs /dlv
```

### Verification Checklist

```powershell
slmgr.vbs /dlv
```

**Expected Output (Grace Period):**
```
License Status: Initial grace period
Grace Period Remaining: N days (typically 30)
→ HOẶC
License Status: Out of tolerance grace period
Grace Period Remaining: N days
```

**Check WMI:**
```powershell
Get-WmiObject -Namespace "root\cimv2" -Class SoftwareLicensingProduct | 
  Where-Object { $_.PartialProductKey } | 
  Select-Object Name, LicenseStatus, GraceRemaining
```

**Expected LicenseStatus = 2-8 (Grace Period variants)**
- 2 = Initial grace period
- 3 = Initial grace with notification
- 4 = Non-genuine grace
- 5 = Non-genuine with notification
- 6 = Out of tolerance grace
- 7 = Out of tolerance with notification
- 8 = Extended grace

### Characteristics to Verify
- ⏱ "Activate Windows" notification visible
- ⚠ Watermark shows (optional based on grace type)
- ✓ System hoạt động bình thường
- ✓ Grace period countdown (N days remaining)
- ✓ PartialProductKey có thể empty hoặc partial

---

## Part 5: VM 4 - Cracked / Non-Genuine License

### Goal
Windows được detect là **non-genuine** hoặc **pirated**.

### Setup Steps (⚠️ Lab Only - Không dùng production)

#### Option A: Dùng KMS Emulator (Simplest - giả lập không genuine)
```powershell
# ⚠️ Chỉ dùng trong lab environment, chứ không phải production

# 1. Cài Windows bình thường

# 2. Download/setup KMS emulator (ví dụ: KMSPico - chỉ lab)
# https://github.com/kkkk123454321/KMS-Server (ví dụ)
# hoặc
# https://github.com/SystemRage/py-kms (Python-based KMS server)

# 3. Set KMS server tới emulator
slmgr.vbs /skms localhost

# 4. Activate
slmgr.vbs /ato

# 5. Modified system files detection:
# Windows sẽ detect KMS emulator → trigger non-genuine flag
```

#### Option B: Dùng Invalid/Pirated Product Key
```powershell
# 1. Cài Windows

# 2. Install invalid product key (fake key)
slmgr.vbs /ipk AAAAA-BBBBB-CCCCC-DDDDD-EEEEE

# 3. Thử activate (sẽ fail)
slmgr.vbs /ato

# 4. Windows sẽ detect → mark as non-genuine
```

#### Option C: Modify System License Files (Advanced)
```powershell
# ⚠️ Rất advanced - yêu cầu deep Windows knowledge
# 1. Disable Windows Genuine Advantage validation
# 2. Modify license metadata files

# Không khuyến nghị vì quá phức tạp - use Option A/B thay
```

### Verification Checklist

```powershell
slmgr.vbs /dlv
```

**Expected Output (Cracked/Non-Genuine):**
```
License Status: Initial grace period (for non-genuine products)
Notification Reason: 0xC004C003 (Non-genuine)
→ HOẶC
Activation ID: <not valid>
```

**Check WMI:**
```powershell
Get-WmiObject -Namespace "root\cimv2" -Class SoftwareLicensingProduct | 
  Where-Object { $_.PartialProductKey } | 
  Select-Object Name, LicenseStatus, NotificationReason
```

**Expected LicenseStatus = 0 hoặc > 8 (Cracked indicator)**

### Characteristics to Verify
- ✗ "Windows is not genuine" watermark visible
- ✗ Updates might be limited
- ✗ Personalization may be disabled
- ✗ PartialProductKey = empty
- 🚨 Notification reason = 0xC004C003 (non-genuine) hoặc tương tự

---

## Part 6: VM 5 - Unable to Determine (Detection Failed)

### Goal
License Checker **không thể xác định** status vì WMI/Registry unavailable.

### Setup Steps

#### Option A: Disable WMI Service
```powershell
# 1. Stop WMI service
Stop-Service -Name "winmgmt" -Force

# 2. Disable WMI service permanently
Set-Service -Name "winmgmt" -StartupType Disabled

# 3. Restart Windows

# 4. License Checker sẽ fail detect via WMI
#    → fallback tới Registry
```

#### Option B: Restrict Registry Access
```powershell
# 1. Modify NTFS permissions trên registry key
# 2. Deny read access tới user running License Checker

# HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\
# Right-click > Properties > Security
# Deny "Read" permission cho user

# 3. License Checker cannot read registry
#    → return UnableToDetermine
```

#### Option C: Corrupt License Service
```powershell
# 1. Stop Software Protection Platform service
Stop-Service -Name "sppsvc" -Force

# 2. Delete/corrupt license database (backup first!)
Remove-Item -Path "C:\Windows\System32\spp\store\*" -Force -Recurse

# ⚠️ Có thể gây Windows không hoạt động - cẩn thận!

# 3. Restart
Restart-Computer

# 4. License status không thể determine
```

#### Option D: Run as Limited User
```powershell
# Nếu License Checker chạy với limited user (không admin)
# Nó sẽ không thể access registry/WMI
# → return UnableToDetermine

# Chạy License Checker với limited account
```

### Verification Checklist

**License Checker output:**
```json
{
  "licenseStatus": 3,
  "licenseStatusDetail": "Unable to Determine",
  "isError": true,
  "errorMessage": "WMI query failed: Access denied" 
             hoặc "Registry key not accessible"
}
```

**Check WMI (if available):**
```powershell
# Nếu WMI disabled, command này sẽ fail:
Get-WmiObject -Namespace "root\cimv2" -Class SoftwareLicensingProduct
# Error: "Generic failure"
```

### Characteristics to Verify
- ✗ WMI query fails
- ✗ Registry read fails
- ✗ License details = empty
- ✓ isError = true
- ✓ errorMessage populated

---

## Part 7: Testing License Checker Agent

### Deployment Steps

#### 1. Copy License Checker Files
```powershell
# Trên host machine, copy files tới VM:
Copy-Item -Path "C:\Path\to\LicenseCheckerAgent.exe" `
          -Destination "\\VM-IP\c$\temp\" -Verbose

Copy-Item -Path "C:\Path\to\LicenseCheckerInstaller.exe" `
          -Destination "\\VM-IP\c$\temp\" -Verbose
```

#### 2. Install Service (mỗi VM)
```powershell
# RDP tới VM
# Run as Administrator:

cd C:\temp

# Option A: Dùng ServiceInstaller
.\LicenseCheckerInstaller.exe install

# Option B: Manual PowerShell
New-Service -Name "LicenseCheckerAgent" `
  -BinaryPathName "C:\temp\LicenseCheckerAgent.exe" `
  -DisplayName "License Checker Agent" `
  -StartupType Automatic

Start-Service -Name "LicenseCheckerAgent"
```

#### 3. Verify Service Running
```powershell
# Check service status
Get-Service -Name "LicenseCheckerAgent" | Format-List

# Output:
# Status: Running
# StartType: Automatic
```

#### 4. Check License Detection Output
```powershell
# Xem log từ Agent (nếu có)
Get-Content -Path "C:\ProgramData\LicenseChecker\log.txt" -Tail 50

# Hoặc check event viewer:
Get-EventLog -LogName "Application" -Source "LicenseCheckerAgent" -Newest 10
```

### Validation Matrix

| VM | Expected Status | Verify Command | Expected Output |
|---|---|---|---|
| VM-Legitimate | Licensed (0) | `slmgr /dlv` | LicenseStatus = 1 |
| VM-NotLicensed | Not Licensed (2) | `slmgr /dlv` | LicenseStatus = 0 |
| VM-GracePeriod | Grace Period (2) | `slmgr /dlv` | LicenseStatus = 2-8 |
| VM-Cracked | Cracked (1) | `slmgr /dlv` | LicenseStatus > 8 or notification |
| VM-Unable | Unable (3) | Agent log | isError = true |

---

## Part 8: Test Checklist

### Per-VM Testing

- [ ] **VM Setup**
  - [ ] Windows installed & updated
  - [ ] Network connectivity verified
  - [ ] RDP enabled
  - [ ] PowerShell execution policy set

- [ ] **License Configuration**
  - [ ] License state set correctly
  - [ ] `slmgr /dlv` output matches expected
  - [ ] WMI query returns expected LicenseStatus value

- [ ] **Agent Deployment**
  - [ ] Agent files copied
  - [ ] Service installed
  - [ ] Service status = Running
  - [ ] No startup errors in event log

- [ ] **Detection Validation**
  - [ ] Agent detects correct license status
  - [ ] License details populated (name, edition, key)
  - [ ] JSON output well-formed
  - [ ] Named Pipe communication working
  - [ ] UI displays correct status

### Cross-VM Testing

- [ ] Dashboard shows all 5 VMs
- [ ] Each VM shows correct license status
- [ ] License violations flagged (Cracked, Not Licensed)
- [ ] Compliance rate calculated correctly
- [ ] Department assignment correct (if multi-dept setup)

### Edge Case Testing

- [ ] VM-Cracked status shows warning icon
- [ ] VM-NotLicensed shows orange status
- [ ] VM-GracePeriod shows countdown (if available)
- [ ] VM-Unable shows error message
- [ ] VM-Legitimate shows green checkmark

---

## Part 9: Troubleshooting

### Issue: WMI Query Returns No Results

**Symptom:**
```
Get-WmiObject returns empty
```

**Solution:**
```powershell
# 1. Check WMI service status
Get-Service -Name "winmgmt"

# 2. Restart WMI
Restart-Service -Name "winmgmt"

# 3. Rebuild WMI repository (last resort)
winmgmt /salvagerepository
Restart-Service -Name "winmgmt"

# 4. Check namespace exists
Get-WmiObject -Namespace "root\cimv2" -Class SoftwareLicensingProduct
```

### Issue: slmgr.vbs Shows "Unknown"

**Symptom:**
```
License Status: Unknown
```

**Solution:**
```powershell
# 1. Update licensing data
slmgr.vbs /ato

# 2. Restart licensing service
Restart-Service -Name "sppsvc"

# 3. Re-query after 5 minutes
slmgr.vbs /dlv
```

### Issue: Agent Fails to Install Service

**Symptom:**
```
Error: Access denied / Service already exists
```

**Solution:**
```powershell
# 1. Run as Administrator
# (Right-click CMD/PowerShell > Run as Administrator)

# 2. Check if service already exists
Get-Service -Name "LicenseCheckerAgent" -ErrorAction SilentlyContinue

# 3. If exists, remove first
Stop-Service -Name "LicenseCheckerAgent" -Force
Remove-Service -Name "LicenseCheckerAgent"

# 4. Then install again
.\LicenseCheckerInstaller.exe install
```

### Issue: License Checker Returns "Unable to Determine"

**Symptom:**
```json
{
  "licenseStatus": 3,
  "isError": true
}
```

**Solution:**
```powershell
# 1. Check if running as Admin
# (Agent needs admin rights to access WMI/Registry)

# 2. Verify WMI accessible
Get-WmiObject -Namespace "root\cimv2" -Class SoftwareLicensingProduct

# 3. Verify Registry accessible
reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion"

# 4. Check event viewer for errors
Get-EventLog -LogName "Application" -Source "LicenseCheckerAgent" -Newest 5
```

---

## Part 10: Quick Reference Commands

### License Status Queries
```powershell
# Full license details
slmgr.vbs /dlv

# Quick status
slmgr.vbs /dli

# Show grace period (if available)
slmgr.vbs /xpr

# Show KMS info
slmgr.vbs /dlv | Select-String -Pattern "Key Management"
```

### WMI Direct Queries
```powershell
# Get all licensing products
Get-WmiObject -Namespace "root\cimv2" -Class SoftwareLicensingProduct

# Get specific status
Get-WmiObject -Namespace "root\cimv2" -Class SoftwareLicensingProduct | 
  Select-Object Name, LicenseStatus, GraceRemaining, PartialProductKey

# Get KMS info
Get-WmiObject -Namespace "root\cimv2" -Class SoftwareLicensingProduct | 
  Select-Object Name, KeyManagementServiceMachine, KeyManagementServicePort
```

### Service Management
```powershell
# Check License Checker service
Get-Service "LicenseCheckerAgent" | Format-List

# Start/Stop service
Start-Service "LicenseCheckerAgent"
Stop-Service "LicenseCheckerAgent"

# View recent logs
Get-EventLog -LogName "Application" -Source "LicenseCheckerAgent" -Newest 20
```

### License Manipulation
```powershell
# Change product key
slmgr.vbs /ipk XXXXX-XXXXX-XXXXX-XXXXX-XXXXX

# Activate
slmgr.vbs /ato

# Clear key
slmgr.vbs /upk

# Rearm (reset grace period - limited uses)
slmgr.vbs /rearm

# Set KMS server
slmgr.vbs /skms kms.server.com

# Clear KMS server
slmgr.vbs /ckms
```

---

## Part 11: Test Schedule

### Phase 1: Setup (Week 1)
- [ ] Day 1-2: Create 5 VMs with base Windows
- [ ] Day 3: Configure VM-Legitimate & VM-NotLicensed
- [ ] Day 4: Configure VM-GracePeriod & VM-Cracked
- [ ] Day 5: Configure VM-Unable & deploy Agent

### Phase 2: Testing (Week 2-3)
- [ ] Day 1: Individual VM testing (license detection)
- [ ] Day 2-3: Agent deployment & service running
- [ ] Day 4: Dashboard integration & visualization
- [ ] Day 5: Cross-VM compliance reporting
- [ ] Day 6-7: Edge cases & troubleshooting

### Phase 3: Validation (Week 4)
- [ ] Full suite regression testing
- [ ] Performance validation (5 VMs concurrently)
- [ ] Documentation & handoff

---

## Summary

| VM | License State | Setup Difficulty | Verification |
|---|---|---|---|
| VM 1 | Legitimate | ⭐ Easy | `slmgr /dlv` → LicenseStatus = 1 |
| VM 2 | Not Licensed | ⭐ Easy | Fresh install, no activation |
| VM 3 | Grace Period | ⭐⭐ Medium | Modify hardware OR rearm |
| VM 4 | Cracked | ⭐⭐⭐ Hard | KMS emulator or invalid key |
| VM 5 | Unable to Determine | ⭐⭐ Medium | Disable WMI/Registry access |

**Estimated Total Setup Time:** 2-3 weeks (including configuration & testing)

Good luck with your testing! 🚀
