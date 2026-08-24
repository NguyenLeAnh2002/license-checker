# Machine Identification Strategies

## Problem Statement

Chỉ dùng **hostname** có vấn đề:
- ❌ Hostname có thể trùng (trong 2 mạng/phòng ban khác nhau)
- ❌ Hostname có thể thay đổi (rename máy)
- ❌ Không phân biệt được 2 máy cùng tên trên 2 site khác nhau
- ❌ Không track máy theo vật lý, chỉ track theo tên

**Solution:** Combine multiple identifiers để tạo unique fingerprint

---

## 6 Strategies for Machine Identification

### Strategy 1: Machine GUID (Best for Large Enterprise)

**What:** Globally Unique Identifier từ Windows
**Pros:** 
- Unique cho mỗi Windows installation
- Không thay đổi khi rename hostname
- Tự động sinh bởi Windows

**Cons:**
- Phải query từ WMI/Registry
- Khó nhớ (string dài 36 ký tự)

**C++ Code:**
```cpp
#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

std::string GetMachineGUID() {
    HKEY hKey;
    LONG result = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Cryptography",
        0, KEY_READ, &hKey);
    
    if (result == ERROR_SUCCESS) {
        char guid[256];
        DWORD size = sizeof(guid);
        result = RegQueryValueExA(hKey, "MachineGuid", NULL, NULL,
                                 (LPBYTE)guid, &size);
        RegCloseKey(hKey);
        
        if (result == ERROR_SUCCESS) {
            return std::string(guid);
        }
    }
    return "";
}

// Example output: 12345678-1234-1234-1234-123456789012
```

**Database Schema Update:**
```sql
ALTER TABLE machines ADD COLUMN machine_guid VARCHAR(36) UNIQUE;
ALTER TABLE machines ADD INDEX idx_machine_guid (machine_guid);
```

**Server Matching:**
```python
# Server side - match by GUID first, hostname second
machine_id = db.query(
    "SELECT id FROM machines WHERE machine_guid = ?",
    (machine_guid,)
)

if not machine_id:
    # Fallback to hostname if GUID not found (new machine)
    machine_id = db.query(
        "SELECT id FROM machines WHERE hostname = ? AND machine_guid IS NULL",
        (hostname,)
    )
```

---

### Strategy 2: MAC Address (Network Adapter)

**What:** Physical MAC address của network adapter
**Pros:**
- Unique per network card
- Identifies hardware
- Useful for network inventory
- Can be combined with IP for redundancy

**Cons:**
- Có thể change nếu replace NIC
- Máy có multiple adapters
- Có thể spoof (though rare in corporate)

**C++ Code:**
```cpp
#include <iphlpapi.h>
#include <winsock2.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

std::vector<std::string> GetMACAddresses() {
    std::vector<std::string> macs;
    
    PIP_ADAPTER_INFO pAdapterInfo = new IP_ADAPTER_INFO();
    ULONG size = sizeof(IP_ADAPTER_INFO);
    
    if (GetAdaptersInfo(pAdapterInfo, &size) == ERROR_BUFFER_OVERFLOW) {
        delete pAdapterInfo;
        pAdapterInfo = (IP_ADAPTER_INFO*)new BYTE[size];
    }
    
    if (GetAdaptersInfo(pAdapterInfo, &size) == NO_ERROR) {
        PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
        
        while (pAdapter) {
            // Filter out loopback and virtual adapters
            if (pAdapter->Type == MIB_IF_TYPE_ETHERNET ||
                pAdapter->Type = IF_TYPE_IEEE80211) {
                
                char mac[18];
                sprintf_s(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
                         pAdapter->Address[0], pAdapter->Address[1],
                         pAdapter->Address[2], pAdapter->Address[3],
                         pAdapter->Address[4], pAdapter->Address[5]);
                
                macs.push_back(std::string(mac));
            }
            pAdapter = pAdapter->Next;
        }
    }
    
    delete[] pAdapterInfo;
    return macs;
}

// Example output: ["00:1A:2B:3C:4D:5E", "AA:BB:CC:DD:EE:FF"]
```

**Database Schema:**
```sql
ALTER TABLE machines ADD COLUMN primary_mac VARCHAR(17);
ALTER TABLE machines ADD COLUMN mac_list TEXT;  -- JSON array
ALTER TABLE machines ADD INDEX idx_primary_mac (primary_mac);
```

---

### Strategy 3: Hardware Serial Number (BIOS/Motherboard)

**What:** Serial number từ BIOS/Motherboard
**Pros:**
- Unique per physical machine
- Doesn't change
- Identifies actual hardware
- Good for asset tracking

**Cons:**
- Not all machines expose this
- Requires WMI/SMBIOS queries
- Can be expensive to query

**C++ Code:**
```cpp
#include <setupapi.h>
#pragma comment(lib, "setupapi.lib")

std::string GetBIOSSerialNumber() {
    HKEY hKey;
    LONG result = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System",
        0, KEY_READ, &hKey);
    
    if (result == ERROR_SUCCESS) {
        char serial[256];
        DWORD size = sizeof(serial);
        
        result = RegQueryValueExA(hKey, "SystemBiosVersion", NULL, NULL,
                                 (LPBYTE)serial, &size);
        
        RegCloseKey(hKey);
        if (result == ERROR_SUCCESS) {
            return std::string(serial);
        }
    }
    
    // Alternative: WMI query
    // SELECT SerialNumber FROM Win32_SystemEnclosure
    return "";
}
```

---

### Strategy 4: Composite ID (Recommended) 🌟

**What:** Combine multiple identifiers into one fingerprint
**Pros:**
- Most reliable (multiple signals)
- Survives hardware changes
- Tracks machine logically
- Can match on subset if needed

**Cons:**
- More complex to implement
- Multiple queries

**Best Practice Hierarchy:**
```
1st Priority: machine_guid (GUID from Windows Cryptography)
2nd Priority: primary_mac (Primary NIC MAC)
3rd Priority: hostname (Fallback)

composite_id = SHA256(machine_guid + primary_mac + hostname)
```

**C++ Code:**
```cpp
#include <string>
#include <sstream>
#include <iomanip>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")

struct MachineFingerprint {
    std::string machine_guid;
    std::string primary_mac;
    std::string hostname;
    std::string composite_id;  // SHA256 hash
};

std::string SHA256Hash(const std::string& input) {
    HCRYPTPROV hProvider = 0;
    HCRYPTHASH hHash = 0;
    
    if (!CryptAcquireContextA(&hProvider, NULL, NULL, PROV_RSA_AES, 0)) {
        return "";
    }
    
    if (!CryptCreateHash(hProvider, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProvider, 0);
        return "";
    }
    
    if (!CryptHashData(hHash, (BYTE*)input.c_str(), input.length(), 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProvider, 0);
        return "";
    }
    
    BYTE hash[32];
    DWORD hashSize = 32;
    
    if (!CryptGetHashParam(hHash, HP_HASHVALUE, hash, &hashSize, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProvider, 0);
        return "";
    }
    
    // Convert to hex string
    std::stringstream ss;
    for (int i = 0; i < hashSize; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProvider, 0);
    
    return ss.str();
}

MachineFingerprint CreateMachineFingerprint() {
    MachineFingerprint fp;
    
    fp.machine_guid = GetMachineGUID();        // From Cryptography registry
    fp.primary_mac = GetPrimaryMACAddress();   // First network adapter
    fp.hostname = GetComputerNameA();          // Windows hostname
    
    // Create composite ID
    std::string input = fp.machine_guid + fp.primary_mac + fp.hostname;
    fp.composite_id = SHA256Hash(input);
    
    return fp;
}

// Example output:
// machine_guid: 12345678-1234-1234-1234-123456789012
// primary_mac: 00:1A:2B:3C:4D:5E
// hostname: DESKTOP-ABC123
// composite_id: a3c9d1e2f4b5c7d9e1f3a5b7c9d1e3f5
```

**JSON Payload to Server:**
```json
{
  "machine_identifiers": {
    "composite_id": "a3c9d1e2f4b5c7d9e1f3a5b7c9d1e3f5",
    "machine_guid": "12345678-1234-1234-1234-123456789012",
    "primary_mac": "00:1A:2B:3C:4D:5E",
    "hostname": "DESKTOP-ABC123",
    "all_macs": ["00:1A:2B:3C:4D:5E", "AA:BB:CC:DD:EE:FF"]
  },
  "os_version": 10,
  "department": "Finance",
  "licenseStatus": "Legitimate",
  "timestamp": "2026-08-06T19:00:00Z"
}
```

**Server Database Schema:**
```sql
ALTER TABLE machines ADD COLUMN composite_id VARCHAR(64) UNIQUE;
ALTER TABLE machines ADD COLUMN machine_guid VARCHAR(36);
ALTER TABLE machines ADD COLUMN primary_mac VARCHAR(17);
ALTER TABLE machines ADD COLUMN all_macs JSON;
ALTER TABLE machines ADD COLUMN hostname VARCHAR(255);

CREATE INDEX idx_composite_id ON machines(composite_id);
CREATE INDEX idx_machine_guid ON machines(machine_guid);
CREATE INDEX idx_primary_mac ON machines(primary_mac);
CREATE INDEX idx_hostname ON machines(hostname);
```

---

### Strategy 5: Active Directory GUID (Enterprise)

**What:** Use AD GUID if machine is domain-joined
**Pros:**
- Enterprise standard
- Integrated with AD
- Perfect for domain environments
- Tracks machine in directory

**Cons:**
- Only works for domain-joined machines
- Non-domain machines won't have this
- Requires AD connectivity

**C++ Code:**
```cpp
#include <lm.h>
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "wldap32.lib")

std::string GetADComputerGUID() {
    LPWSTR pszAttribute[] = {L"objectGUID"};
    
    HANDLE hDs = NULL;
    if (DsBind(NULL, NULL, &hDs) != ERROR_SUCCESS) {
        return "";  // Not domain-joined
    }
    
    WCHAR computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    if (!GetComputerNameW(computerName, &size)) {
        DsUnBind(&hDs);
        return "";
    }
    
    SEARCH_HANDLE hSearch;
    LPBYTE pObjectGUID = NULL;
    USHORT len = 0;
    
    if (DsSearchFindFirst(hDs, NULL, NULL, 1, pszAttribute, 1,
                         &hSearch) != ERROR_SUCCESS) {
        DsUnBind(&hDs);
        return "";
    }
    
    // Convert GUID to string
    std::string guid = "";
    // ... GUID formatting logic ...
    
    DsSearchClose(hSearch);
    DsUnBind(&hDs);
    
    return guid;
}
```

**Detection Logic:**
```python
# Server: Try AD GUID first, fallback to composite ID
def identify_machine(request_data):
    identifiers = request_data.get('machine_identifiers', {})
    
    # Try AD GUID first
    if identifiers.get('ad_guid'):
        machine = db.query(
            "SELECT id FROM machines WHERE ad_guid = ?",
            (identifiers['ad_guid'],)
        )
        if machine:
            return machine[0]
    
    # Fallback to composite ID
    if identifiers.get('composite_id'):
        machine = db.query(
            "SELECT id FROM machines WHERE composite_id = ?",
            (identifiers['composite_id'],)
        )
        if machine:
            return machine[0]
    
    # Fallback to MAC address
    if identifiers.get('primary_mac'):
        machine = db.query(
            "SELECT id FROM machines WHERE primary_mac = ?",
            (identifiers['primary_mac'],)
        )
        if machine:
            return machine[0]
    
    # Last resort: hostname
    if identifiers.get('hostname'):
        machine = db.query(
            "SELECT id FROM machines WHERE hostname = ?",
            (identifiers['hostname'],)
        )
        if machine:
            return machine[0]
    
    # No match - create new machine
    return create_new_machine(identifiers)
```

---

### Strategy 6: Combination Smart Matching

**What:** Multi-level matching with fallback logic
**Pros:**
- Handles all scenarios
- Survives hardware changes
- Handles network changes
- Perfect for real-world deployment

**Cons:**
- More complex matching logic

**Decision Tree:**

```
┌─ GUID match?
│  ├─ YES → Use existing machine (GUID is most reliable)
│  └─ NO
│      ├─ MAC match?
│      │  ├─ YES → Use existing machine (MAC is reliable)
│      │  └─ NO
│      │      ├─ Composite ID match?
│      │      │  ├─ YES → Use existing machine
│      │      │  └─ NO
│      │      │      ├─ Hostname + Department match?
│      │      │      │  ├─ YES → Use existing machine (maybe rename/hardware change)
│      │      │      │  └─ NO → CREATE NEW MACHINE
└─ (All signals aligned)
```

**Python Implementation:**
```python
def smart_machine_matching(db, identifiers):
    """
    Match machine using multiple strategies with fallback logic
    Returns: (machine_id, match_confidence, match_type)
    """
    
    composite_id = identifiers.get('composite_id')
    machine_guid = identifiers.get('machine_guid')
    primary_mac = identifiers.get('primary_mac')
    hostname = identifiers.get('hostname')
    department = identifiers.get('department', 'Default')
    
    # Level 1: Composite ID (most reliable, 100% confident)
    if composite_id:
        machine = db.query(
            "SELECT id FROM machines WHERE composite_id = ?",
            (composite_id,)
        )
        if machine:
            return (machine[0], 1.0, "composite_id")
    
    # Level 2: Machine GUID (very reliable, 95% confident)
    if machine_guid:
        machine = db.query(
            "SELECT id FROM machines WHERE machine_guid = ?",
            (machine_guid,)
        )
        if machine:
            return (machine[0], 0.95, "machine_guid")
    
    # Level 3: Primary MAC (reliable, 90% confident)
    if primary_mac:
        machine = db.query(
            "SELECT id FROM machines WHERE primary_mac = ? AND department = ?",
            (primary_mac, department)
        )
        if machine:
            return (machine[0], 0.90, "primary_mac")
    
    # Level 4: Hostname + Department (moderate, 70% confident)
    if hostname:
        machine = db.query(
            "SELECT id FROM machines WHERE hostname = ? AND department = ?",
            (hostname, department)
        )
        if machine:
            # Log warning - hostname only should not be primary
            log_warning(f"Machine matched by hostname only: {hostname}")
            return (machine[0], 0.70, "hostname")
    
    # Level 5: No match - create new machine
    return (None, 0.0, "new_machine")

# Usage in agent report handler
def handle_license_report(request_data):
    identifiers = request_data.get('machine_identifiers', {})
    machine_id, confidence, match_type = smart_machine_matching(db, identifiers)
    
    if confidence < 0.50:
        # Log suspicious - multiple identifiers don't match
        log_warning(f"Low confidence match ({confidence}): {identifiers}")
    
    if machine_id is None:
        # Create new machine
        machine_id = create_new_machine(identifiers)
    
    update_license_status(machine_id, request_data)
    return machine_id
```

---

## Comparison Table

| Strategy | Reliability | Uniqueness | Persistence | Implementation | Cost |
|----------|-------------|-----------|--------------|-----------------|------|
| **Hostname Only** | ⭐ Low | ⭐ Low | ⭐ Low (can change) | ⭐ Easy | Free |
| **Machine GUID** | ⭐⭐⭐⭐⭐ Very High | ⭐⭐⭐⭐⭐ Very High | ⭐⭐⭐⭐⭐ Permanent | ⭐⭐⭐ Medium | Free |
| **MAC Address** | ⭐⭐⭐⭐ High | ⭐⭐⭐⭐ High | ⭐⭐⭐⭐ High | ⭐⭐ Moderate | Free |
| **BIOS Serial** | ⭐⭐⭐⭐⭐ Very High | ⭐⭐⭐⭐⭐ Very High | ⭐⭐⭐⭐⭐ Permanent | ⭐⭐⭐⭐ Complex | Free |
| **Composite ID** | ⭐⭐⭐⭐⭐ Very High | ⭐⭐⭐⭐⭐ Very High | ⭐⭐⭐⭐⭐ Permanent | ⭐⭐⭐ Medium | Free |
| **AD GUID** | ⭐⭐⭐⭐⭐ Very High | ⭐⭐⭐⭐⭐ Very High | ⭐⭐⭐⭐⭐ Permanent | ⭐⭐⭐ Medium | Requires AD |

---

## Recommended Implementation Strategy

### For Most Organizations (Hybrid Approach):

```json
{
  "machine_identifiers": {
    "level_1_composite_id": "a3c9d1e2f4b5c7d9e1f3a5b7c9d1e3f5",
    "level_2_machine_guid": "12345678-1234-1234-1234-123456789012",
    "level_3_primary_mac": "00:1A:2B:3C:4D:5E",
    "level_4_hostname": "DESKTOP-ABC123",
    "ad_guid": "87654321-4321-4321-4321-210987654321"
  },
  "metadata": {
    "department": "Finance",
    "os_version": 10,
    "os_edition": "Pro",
    "is_domain_joined": true
  }
}
```

### Server Matching Priority:
1. **Composite ID** (SHA256 of GUID+MAC+Hostname) → 100% confident
2. **Machine GUID** → 95% confident
3. **Primary MAC** → 90% confident  
4. **AD GUID** (if domain-joined) → 95% confident
5. **Hostname + Department** → 70% confident (warning)
6. **No match** → Create new machine

---

## Database Schema Update

```sql
-- Enhanced machines table
ALTER TABLE machines ADD COLUMN composite_id VARCHAR(64) UNIQUE;
ALTER TABLE machines ADD COLUMN machine_guid VARCHAR(36) UNIQUE;
ALTER TABLE machines ADD COLUMN primary_mac VARCHAR(17) UNIQUE;
ALTER TABLE machines ADD COLUMN all_macs JSON;
ALTER TABLE machines ADD COLUMN ad_guid VARCHAR(36);
ALTER TABLE machines ADD COLUMN bios_serial VARCHAR(255);
ALTER TABLE machines ADD COLUMN is_domain_joined BOOLEAN DEFAULT 0;
ALTER TABLE machines ADD COLUMN last_hardware_change TIMESTAMP;
ALTER TABLE machines ADD COLUMN identification_confidence FLOAT DEFAULT 1.0;
ALTER TABLE machines ADD COLUMN identification_method VARCHAR(50);

-- Create indexes for performance
CREATE UNIQUE INDEX idx_composite_id ON machines(composite_id);
CREATE INDEX idx_machine_guid ON machines(machine_guid);
CREATE INDEX idx_primary_mac ON machines(primary_mac);
CREATE INDEX idx_ad_guid ON machines(ad_guid);
CREATE INDEX idx_hostname_dept ON machines(hostname, department_id);

-- Track identification history
CREATE TABLE machine_identifiers_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    machine_id INTEGER NOT NULL,
    composite_id VARCHAR(64),
    machine_guid VARCHAR(36),
    primary_mac VARCHAR(17),
    hostname VARCHAR(255),
    change_type VARCHAR(50),  -- 'RENAME', 'HARDWARE_CHANGE', 'NEW', etc.
    detected_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (machine_id) REFERENCES machines(id)
);
```

---

## Server-side Change Detection

```python
def detect_machine_changes(db, machine_id, new_identifiers):
    """
    Track when machine changes identifiers
    """
    old_data = db.query(
        "SELECT composite_id, machine_guid, primary_mac, hostname FROM machines WHERE id = ?",
        (machine_id,)
    )[0]
    
    changes = []
    
    # Check for hostname change
    if old_data['hostname'] != new_identifiers['hostname']:
        changes.append({
            'type': 'HOSTNAME_CHANGE',
            'old': old_data['hostname'],
            'new': new_identifiers['hostname']
        })
    
    # Check for MAC change (hardware replacement)
    if old_data['primary_mac'] != new_identifiers['primary_mac']:
        changes.append({
            'type': 'HARDWARE_CHANGE',
            'old_mac': old_data['primary_mac'],
            'new_mac': new_identifiers['primary_mac']
        })
    
    # Log changes
    if changes:
        db.query(
            """INSERT INTO machine_identifiers_history 
               (machine_id, change_type, detected_at)
               VALUES (?, ?, CURRENT_TIMESTAMP)""",
            (machine_id, ','.join([c['type'] for c in changes]))
        )
        
        # Alert admin if suspicious changes
        if len(changes) > 1:
            log_warning(f"Multiple changes detected for machine {machine_id}: {changes}")
```

---

## Summary Recommendations

| Scenario | Best Strategy |
|----------|---------------|
| **Small Organization (<100 machines)** | Composite ID (GUID + MAC + Hostname) |
| **Medium Organization (100-1000 machines)** | Composite ID + Smart Matching |
| **Large Enterprise** | Composite ID + AD GUID + Smart Matching |
| **Multi-site/Multi-network** | Composite ID + Department Scoping |
| **High Security** | Composite ID + Hardware Serial + Audit Trail |

---

## Implementation Checklist

- [ ] Add composite_id to database
- [ ] Add machine_guid column
- [ ] Add primary_mac column
- [ ] Add ad_guid column (if enterprise)
- [ ] Update agent to send all identifiers
- [ ] Implement smart matching on server
- [ ] Add change detection logic
- [ ] Create audit trail table
- [ ] Test with hostname changes
- [ ] Test with hardware changes
- [ ] Document for operations team
- [ ] Train support staff on identifier meanings

---

**This comprehensive approach ensures no two machines are confused, even across complex enterprise environments!** 🎯
