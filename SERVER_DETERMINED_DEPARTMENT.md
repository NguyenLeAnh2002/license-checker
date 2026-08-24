# Server-Determined Department Assignment

## Philosophy
**Server quyết định phòng ban**, agent chỉ gửi lên các thông tin định danh (GUID, MAC, hostname, IP). Server tự động xác định phòng ban dựa trên các chiến lược khác nhau.

**Lợi ích:**
- ✅ Server là source of truth
- ✅ Admin quản lý tập trung (1 chỗ)
- ✅ Dễ thay đổi phòng ban (không cần update agent)
- ✅ Scalable (100+ machines)
- ✅ Consistent logic
- ✅ Audit trail tự động

---

## 5 Server-Side Strategies

### Strategy 1: Active Directory (Best for Enterprise) ⭐⭐⭐⭐⭐

**How it works:**
- Agent gửi: hostname + GUID + MAC
- Server query AD với hostname/GUID
- AD trả về: department (OU structure)
- Server lưu department

**Server Code (Python):**
```python
import ldap

class ActiveDirectoryResolver:
    def __init__(self, ad_server, ad_domain, service_account, password):
        self.ad_server = ad_server
        self.ad_domain = ad_domain
        self.conn = ldap.initialize(f'ldap://{ad_server}')
        self.conn.simple_bind_s(service_account, password)
        self.base_dn = self._generate_base_dn(ad_domain)
    
    def _generate_base_dn(self, domain):
        """
        Convert corp.local → DC=corp,DC=local
        """
        parts = domain.split('.')
        return ','.join([f'DC={part}' for part in parts])
    
    def get_department_by_hostname(self, hostname):
        """
        Query AD để tìm phòng ban dựa vào hostname
        
        AD Structure:
        DC=corp,DC=local
        └─ OU=Finance
            └─ CN=DESKTOP-F001
        └─ OU=IT
            └─ CN=LAPTOP-IT001
        """
        try:
            # Search for computer object
            search_filter = f"(&(objectClass=computer)(name={hostname}))"
            result = self.conn.search_s(
                self.base_dn,
                ldap.SCOPE_SUBTREE,
                search_filter,
                ['department', 'distinguishedName']
            )
            
            if result:
                dn, attrs = result[0]
                
                # Method 1: Get from department attribute
                if b'department' in attrs:
                    dept = attrs[b'department'][0].decode('utf-8')
                    return dept
                
                # Method 2: Extract from DN (OU=Finance,...)
                # DN example: CN=DESKTOP-F001,OU=Finance,OU=Computers,DC=corp,DC=local
                if dn:
                    dept = self._extract_department_from_dn(dn)
                    if dept:
                        return dept
            
            return None
        
        except ldap.NO_SUCH_OBJECT:
            return None
        except Exception as e:
            print(f"AD Error: {e}")
            return None
    
    def _extract_department_from_dn(self, dn):
        """
        Extract department from Distinguished Name
        CN=DESKTOP-F001,OU=Finance,OU=Computers,DC=corp,DC=local
        → Extract: Finance
        """
        parts = dn.split(',')
        for part in parts:
            if part.startswith('OU='):
                ou_name = part.replace('OU=', '').strip()
                # Skip technical OUs
                if ou_name.lower() not in ['computers', 'users', 'workstations']:
                    return ou_name
        return None
    
    def get_department_by_guid(self, machine_guid):
        """
        Query AD by machine GUID (more reliable than hostname)
        """
        try:
            search_filter = f"(objectGUID={machine_guid})"
            result = self.conn.search_s(
                self.base_dn,
                ldap.SCOPE_SUBTREE,
                search_filter,
                ['department', 'cn', 'distinguishedName']
            )
            
            if result:
                dn, attrs = result[0]
                hostname = attrs[b'cn'][0].decode('utf-8')
                
                if b'department' in attrs:
                    dept = attrs[b'department'][0].decode('utf-8')
                    return dept
                
                dept = self._extract_department_from_dn(dn)
                return dept
            
            return None
        
        except Exception as e:
            print(f"AD GUID lookup error: {e}")
            return None

# FastAPI Server
from fastapi import FastAPI

app = FastAPI()

# Initialize AD resolver
ad_resolver = ActiveDirectoryResolver(
    ad_server="corp-ad.local",
    ad_domain="corp.local",
    service_account="svc_license_checker@corp.local",
    password="password123"
)

@app.post("/api/agent/report-license")
async def report_license(data: dict):
    """
    Agent gửi: GUID + hostname + MAC
    Server xác định: department từ AD
    """
    machine_guid = data['machine_guid']
    hostname = data['hostname']
    primary_mac = data['primary_mac']
    
    # Step 1: Try to get department from AD (by GUID first, then hostname)
    department = ad_resolver.get_department_by_guid(machine_guid)
    
    if not department:
        department = ad_resolver.get_department_by_hostname(hostname)
    
    if not department:
        department = "Unknown"  # Fallback
    
    # Step 2: Find or create machine
    machine = db.query(
        "SELECT id FROM machines WHERE machine_guid = ?",
        (machine_guid,)
    )
    
    if machine:
        machine_id = machine[0]
        # Update department if changed in AD
        db.query(
            "UPDATE machines SET department = ?, last_ad_sync = NOW() WHERE id = ?",
            (department, machine_id)
        )
    else:
        # Create new machine
        db.query("""
            INSERT INTO machines (machine_guid, hostname, primary_mac, department, last_ad_sync)
            VALUES (?, ?, ?, ?, NOW())
        """, (machine_guid, hostname, primary_mac, department))
        machine_id = db.lastrowid
    
    # Step 3: Update license status
    update_license(machine_id, data)
    
    return {
        "status": "received",
        "machine_id": machine_id,
        "department": department
    }
```

**Advantages:**
- ✅ Automatic (no manual work)
- ✅ Always up-to-date (AD is source)
- ✅ Scales perfectly
- ✅ Standard enterprise approach

**Disadvantages:**
- ❌ Requires AD setup
- ❌ Non-domain machines not supported

---

### Strategy 2: Hostname Pattern Matching ⭐⭐⭐

**How it works:**
- Define hostname patterns per department
- Server matches hostname against patterns
- Example: Desktop-F* → Finance, Laptop-IT* → IT

**Server Code:**
```python
class HostnameBasedDepartmentResolver:
    def __init__(self):
        # Define hostname patterns
        self.patterns = {
            "Finance": [
                "DESKTOP-F*",      # DESKTOP-F001, DESKTOP-F002
                "LAPTOP-F*",
                "*-FIN-*",
                "WS-FINANCE-*"
            ],
            "IT": [
                "DESKTOP-IT*",
                "LAPTOP-IT*",
                "SRV-IT-*",
                "*-SYSADMIN-*"
            ],
            "HR": [
                "DESKTOP-H*",
                "LAPTOP-HR*",
                "*-HUMAN-*"
            ],
            "Sales": [
                "DESKTOP-S*",
                "LAPTOP-S*",
                "*-SALES-*"
            ]
        }
    
    def get_department(self, hostname):
        """
        Match hostname against patterns
        """
        import fnmatch
        
        hostname_upper = hostname.upper()
        
        for department, patterns in self.patterns.items():
            for pattern in patterns:
                if fnmatch.fnmatch(hostname_upper, pattern.upper()):
                    return department
        
        return "Unknown"

# Usage in FastAPI
hostname_resolver = HostnameBasedDepartmentResolver()

@app.post("/api/agent/report-license")
async def report_license(data: dict):
    hostname = data['hostname']
    
    # Get department from hostname pattern
    department = hostname_resolver.get_department(hostname)
    
    # Rest of processing...
    # Save to database, update license, etc.
```

**Examples:**
```
Hostname Pattern          → Department
DESKTOP-F001           → Finance
LAPTOP-FINANCE-001     → Finance
WS-FIN-A01             → Finance
DESKTOP-IT001          → IT
SRV-IT-PROD            → IT
LAPTOP-HR-002          → HR
DESKTOP-SALES-05       → Sales
```

**Advantages:**
- ✅ Simple (no AD needed)
- ✅ Works with any machine
- ✅ Easy to maintain

**Disadvantages:**
- ❌ Requires naming convention
- ❌ Won't work if hostname doesn't match

---

### Strategy 3: MAC Address Range Mapping ⭐⭐⭐⭐

**How it works:**
- Map MAC address ranges to departments
- Based on network segments
- Example: 00:1A:2B:* → Finance (Finance subnet)

**Server Code:**
```python
class MACBasedDepartmentResolver:
    def __init__(self):
        # MAC address prefixes per department
        # (First 6 chars of MAC = vendor, use for network segment)
        self.mac_ranges = {
            "Finance": [
                "00:1A:2B",  # Finance VLAN MAC prefix
                "00:1A:2C",
                "AA:BB:CC"
            ],
            "IT": [
                "00:1A:2D",
                "00:1A:2E",
                "DD:EE:FF"
            ],
            "HR": [
                "00:1A:2F",
                "00:1A:30",
                "11:22:33"
            ]
        }
    
    def get_department(self, primary_mac):
        """
        Match MAC address against ranges
        """
        mac_prefix = primary_mac[:8].upper()  # First 3 octets
        
        for department, ranges in self.mac_ranges.items():
            for mac_range in ranges:
                if mac_prefix == mac_range.upper():
                    return department
        
        return "Unknown"

# Advanced: MAC OUI lookup (Organizationally Unique Identifier)
class AdvancedMACDepartmentResolver:
    def __init__(self):
        # Map network segments to departments
        # Based on DHCP/VLAN assignment
        self.network_map = {
            "Finance": [
                "192.168.10.0/24",   # Finance subnet
                "192.168.11.0/24"
            ],
            "IT": [
                "192.168.20.0/24",   # IT subnet
                "192.168.21.0/24"
            ],
            "HR": [
                "192.168.30.0/24",   # HR subnet
            ]
        }
    
    def get_department_by_ip(self, ip_address):
        """
        Match IP address to department (requires agent to send IP too)
        """
        from ipaddress import ip_address, ip_network
        
        try:
            machine_ip = ip_address(ip_address)
            
            for department, networks in self.network_map.items():
                for network_range in networks:
                    if machine_ip in ip_network(network_range):
                        return department
            
            return "Unknown"
        except:
            return "Unknown"
```

**Advantages:**
- ✅ Based on actual hardware
- ✅ Works without naming convention
- ✅ Network-segment aware

**Disadvantages:**
- ❌ Requires MAC/IP mapping
- ❌ Changes if machine moves network

---

### Strategy 4: Manual Database Mapping ⭐⭐

**How it works:**
- Admin manually map GUID → Department
- Stored in database
- Server looks up in database

**Database Schema:**
```sql
CREATE TABLE machine_department_mappings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    machine_guid VARCHAR(36) UNIQUE,
    department VARCHAR(255),
    mapped_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    mapped_by VARCHAR(255),
    notes TEXT
);

-- Indexes for fast lookup
CREATE UNIQUE INDEX idx_guid_mapping ON machine_department_mappings(machine_guid);
```

**Server Code:**
```python
class ManualDepartmentResolver:
    def get_department(self, machine_guid):
        """
        Look up department from manual mapping table
        """
        mapping = db.query("""
            SELECT department FROM machine_department_mappings 
            WHERE machine_guid = ?
        """, (machine_guid,))
        
        return mapping[0] if mapping else None

@app.post("/api/agent/report-license")
async def report_license(data: dict):
    machine_guid = data['machine_guid']
    hostname = data['hostname']
    
    # Try to get department from manual mapping
    resolver = ManualDepartmentResolver()
    department = resolver.get_department(machine_guid)
    
    if not department:
        # If not mapped, create unassigned record
        department = "Unassigned"
        # Log for admin to review
        log_unassigned_machine(machine_guid, hostname)
    
    # Rest of processing...
```

**Dashboard Admin Interface:**
```html
<!-- Machine Management -->
<div id="machine-management">
    <h3>Unassigned Machines (23)</h3>
    
    <table>
        <tr>
            <th>GUID</th>
            <th>Hostname</th>
            <th>MAC</th>
            <th>Department</th>
        </tr>
        <tr>
            <td>12345678-1234-...</td>
            <td>DESKTOP-NEW-001</td>
            <td>00:1A:2B:3C:4D:5E</td>
            <td>
                <select>
                    <option>-- Select --</option>
                    <option>Finance</option>
                    <option>IT</option>
                    <option>HR</option>
                </select>
                <button onclick="assignDept(guid)">Assign</button>
            </td>
        </tr>
    </table>
</div>
```

**Advantages:**
- ✅ 100% accurate
- ✅ Full control

**Disadvantages:**
- ❌ Manual work per machine
- ❌ Doesn't scale

---

### Strategy 5: Hybrid (Recommended) ⭐⭐⭐⭐⭐

**How it works:**
1. Try AD first (if available)
2. If not found, try hostname pattern
3. If still not found, try MAC range
4. If all fail, mark as "Unassigned" for manual mapping

**Server Code:**
```python
class HybridDepartmentResolver:
    def __init__(self):
        self.ad_resolver = ActiveDirectoryResolver(...)
        self.hostname_resolver = HostnameBasedDepartmentResolver()
        self.mac_resolver = MACBasedDepartmentResolver()
    
    def get_department(self, machine_guid, hostname, primary_mac, ip_address=None):
        """
        Multi-strategy resolver with fallback chain
        """
        # Strategy 1: Active Directory (most reliable)
        department = self.ad_resolver.get_department_by_guid(machine_guid)
        if department:
            return department, "AD"
        
        # Strategy 2: Hostname pattern matching
        department = self.hostname_resolver.get_department(hostname)
        if department != "Unknown":
            return department, "HOSTNAME"
        
        # Strategy 3: MAC address range
        department = self.mac_resolver.get_department(primary_mac)
        if department != "Unknown":
            return department, "MAC"
        
        # Strategy 4: IP address range (if provided)
        if ip_address:
            department = self.mac_resolver.get_department_by_ip(ip_address)
            if department != "Unknown":
                return department, "IP_RANGE"
        
        # Strategy 5: Manual mapping (database)
        mapping = db.query("""
            SELECT department FROM machine_department_mappings 
            WHERE machine_guid = ?
        """, (machine_guid,))
        if mapping:
            return mapping[0], "MANUAL"
        
        # Fallback: Unassigned
        return "Unassigned", "NONE"

# FastAPI
hybrid_resolver = HybridDepartmentResolver()

@app.post("/api/agent/report-license")
async def report_license(data: dict):
    machine_guid = data['machine_guid']
    hostname = data['hostname']
    primary_mac = data['primary_mac']
    ip_address = data.get('ip_address')
    
    # Resolve department using all strategies
    department, resolution_method = hybrid_resolver.get_department(
        machine_guid, hostname, primary_mac, ip_address
    )
    
    # Find or create machine
    machine = db.query(
        "SELECT id FROM machines WHERE machine_guid = ?",
        (machine_guid,)
    )
    
    if machine:
        machine_id = machine[0]
        # Update department if changed
        db.query("""
            UPDATE machines 
            SET department = ?, resolution_method = ?, last_update = NOW()
            WHERE id = ?
        """, (department, resolution_method, machine_id))
    else:
        # Create new machine
        db.query("""
            INSERT INTO machines 
            (machine_guid, hostname, primary_mac, department, resolution_method)
            VALUES (?, ?, ?, ?, ?)
        """, (machine_guid, hostname, primary_mac, department, resolution_method))
        machine_id = db.lastrowid
    
    # Log how department was resolved
    db.query("""
        INSERT INTO department_resolution_log
        (machine_id, department, method, timestamp)
        VALUES (?, ?, ?, NOW())
    """, (machine_id, department, resolution_method))
    
    # Update license...
    update_license(machine_id, data)
    
    return {
        "status": "received",
        "machine_id": machine_id,
        "department": department,
        "resolution_method": resolution_method
    }
```

---

## Comparison Table

| Strategy | Complexity | Accuracy | Automation | Scalability | Setup Required |
|----------|-----------|----------|-----------|------------|-----------------|
| **Active Directory** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ✅ Auto | ⭐⭐⭐⭐⭐ | AD + LDAP |
| **Hostname Pattern** | ⭐⭐ | ⭐⭐⭐ | ✅ Auto | ⭐⭐⭐ | Naming convention |
| **MAC Range** | ⭐⭐⭐ | ⭐⭐⭐⭐ | ✅ Auto | ⭐⭐⭐ | Network mapping |
| **Manual Mapping** | ⭐ | ⭐⭐⭐⭐⭐ | ❌ Manual | ⭐ | None |
| **Hybrid (Recommended)** | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ✅ Auto + Manual | ⭐⭐⭐⭐ | All above |

---

## RECOMMENDED: Hybrid Strategy

**Server Decision Tree:**

```
Agent sends: GUID + hostname + MAC + IP
                ↓
        ┌─ Try Active Directory?
        │  ├─ YES → Found? Use it
        │  └─ NO  → Try hostname
        │
        ├─ Try Hostname Pattern?
        │  ├─ Matched? Use it
        │  └─ No match → Try MAC
        │
        ├─ Try MAC Range?
        │  ├─ Matched? Use it
        │  └─ No match → Try IP
        │
        ├─ Try IP Range?
        │  ├─ Matched? Use it
        │  └─ No match → Try manual
        │
        ├─ Try Manual Mapping?
        │  ├─ Found? Use it
        │  └─ Not found → Use Unassigned
        │
        └─ Store in database
           Log resolution method
           Display in dashboard
```

---

## Agent-Side (Simple)

Agent chỉ gửi **identification data**, không gửi department:

```json
{
  "machine_guid": "12345678-1234-1234-1234-123456789012",
  "hostname": "DESKTOP-F001",
  "primary_mac": "00:1A:2B:3C:4D:5E",
  "ip_address": "192.168.10.50",
  "os_version": 10,
  "licenseStatus": "Legitimate",
  "timestamp": "2026-08-06T19:00:00Z"
}
```

**Note:** No `department` field - Server tự quyết định!

---

## Database Schema

```sql
-- Machines table (updated)
ALTER TABLE machines ADD COLUMN resolution_method VARCHAR(50);
-- Values: 'AD', 'HOSTNAME', 'MAC', 'IP_RANGE', 'MANUAL', 'UNASSIGNED'

-- Resolution log (audit trail)
CREATE TABLE department_resolution_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    machine_id INTEGER NOT NULL,
    department VARCHAR(255),
    method VARCHAR(50),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (machine_id) REFERENCES machines(id)
);

-- Manual mappings (for unassigned machines)
CREATE TABLE machine_department_mappings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    machine_guid VARCHAR(36) UNIQUE,
    department VARCHAR(255),
    mapped_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    mapped_by VARCHAR(255),
    notes TEXT
);

-- Create indexes
CREATE INDEX idx_resolution_method ON machines(resolution_method);
CREATE INDEX idx_machine_guid_mapping ON machine_department_mappings(machine_guid);
```

---

## Dashboard Features

### 1. Resolution Method Display
```html
<table>
    <tr>
        <th>Machine</th>
        <th>Department</th>
        <th>Determined By</th>
    </tr>
    <tr>
        <td>DESKTOP-F001</td>
        <td>Finance</td>
        <td>🏢 Active Directory</td>
    </tr>
    <tr>
        <td>LAPTOP-IT005</td>
        <td>IT</td>
        <td>🏷️ Hostname Pattern</td>
    </tr>
    <tr>
        <td>WORKSTATION-NEW</td>
        <td>Unassigned</td>
        <td>❓ Needs Manual</td>
    </tr>
</table>
```

### 2. Unassigned Machines Management
```html
<div id="unassigned">
    <h3>Machines Pending Assignment (7)</h3>
    
    <table>
        <tr>
            <th>GUID</th>
            <th>Hostname</th>
            <th>MAC</th>
            <th>Why Unassigned</th>
            <th>Action</th>
        </tr>
        <tr>
            <td>abc123...</td>
            <td>NEW-WORKSTATION</td>
            <td>AA:BB:CC:DD:EE:FF</td>
            <td>No AD, no pattern match, no MAC range</td>
            <td>
                <select onchange="assignDept(this)">
                    <option>-- Assign --</option>
                    <option>Finance</option>
                    <option>IT</option>
                    <option>HR</option>
                </select>
            </td>
        </tr>
    </table>
</div>
```

### 3. Resolution Log
```html
<div id="resolution-log">
    <h3>How Departments Are Being Determined</h3>
    
    <div class="stats">
        <div>🏢 From AD: 45 machines (45%)</div>
        <div>🏷️ From Hostname: 32 machines (32%)</div>
        <div>🌐 From MAC Range: 15 machines (15%)</div>
        <div>👤 Manual Mapping: 8 machines (8%)</div>
        <div>❓ Unassigned: 5 machines (5%)</div>
    </div>
</div>
```

---

## Setup Checklist

### For AD Resolution:
- [ ] Set up LDAP connection to AD server
- [ ] Create service account (svc_license_checker@corp.local)
- [ ] Test AD queries
- [ ] Configure base DN
- [ ] Handle AD unavailability gracefully

### For Hostname Pattern:
- [ ] Define naming conventions
- [ ] Document patterns
- [ ] Test pattern matching
- [ ] Share with IT team

### For MAC Range:
- [ ] Map MAC prefixes to departments
- [ ] Or map network subnets to departments
- [ ] Document the mapping
- [ ] Test with sample MACs

### For Hybrid:
- [ ] Implement all strategies
- [ ] Set priority order
- [ ] Log resolution method
- [ ] Monitor unassigned machines
- [ ] Dashboard shows method

---

## Real-World Workflow

```
Day 1: New machine deployed
├─ Agent boots, sends: GUID + hostname + MAC + IP
├─ Server tries: AD lookup → Not found
├─ Server tries: Hostname (NEW-WORKSTATION) → Not found
├─ Server tries: MAC range → Not found
├─ Server tries: IP range (192.168.10.50) → Not found
├─ Server marks: Unassigned
└─ Dashboard alerts: "5 unassigned machines"

Day 1 Afternoon: Admin assigns machine
├─ Admin opens Dashboard → Unassigned machines
├─ Sees: NEW-WORKSTATION → Selects "Finance"
├─ Server saves: machine_department_mappings[GUID] = Finance
└─ Next report: Machine now shows Finance

Day 2: Machine added to AD
├─ Admin updates AD: NEW-WORKSTATION → OU=Finance
├─ Agent sends next report
├─ Server tries: AD lookup → Found! Finance
├─ Server sees: Manual mapping was Finance → Still Finance
├─ Server logs: "Confirmed by AD after manual mapping"
└─ Everything consistent
```

---

## Summary

**Server là người quyết định department:**

✅ **Agent gửi:** GUID + hostname + MAC + IP (identification only)

✅ **Server xác định:** dùng 5 chiến lược theo thứ tự ưu tiên
1. Active Directory (if available)
2. Hostname pattern
3. MAC range
4. IP range
5. Manual mapping

✅ **Dashboard quản lý:**
- Show resolution method
- Manage unassigned machines
- Manual override if needed

✅ **Result:**
- Server là source of truth
- Tập trung quản lý
- Automatic + manual fallback
- Fully auditable
- Scales perfectly

**This is the cleanest approach!** 🎯
