# Department Assignment Strategies

## Problem
Nếu dùng **GUID để định danh máy**, làm sao server biết máy đó ở phòng ban nào?

---

## 4 Strategies

### Strategy 1: Agent Config File (Simplest) ⭐⭐

**How it works:**
- Admin tạo config file trên máy với department info
- Agent đọc từ config, gửi lên server
- Server lưu vào database

**Agent Config File:**
```json
// C:\Program Files\LicenseChecker\config.json
{
  "machine_info": {
    "hostname": "auto",
    "department": "Finance",
    "site": "Hanoi",
    "cost_center": "CC-001"
  },
  "server": {
    "url": "http://server:8000",
    "api_key": "key-12345"
  }
}
```

**C++ Code (Agent):**
```cpp
#include <fstream>
#include <json.hpp>

struct MachineConfig {
    std::string department;
    std::string site;
    std::string cost_center;
};

MachineConfig LoadConfig(const std::string& config_file) {
    std::ifstream file(config_file);
    nlohmann::json config = nlohmann::json::parse(file);
    
    MachineConfig cfg;
    cfg.department = config["machine_info"]["department"].get<std::string>();
    cfg.site = config["machine_info"]["site"].get<std::string>();
    cfg.cost_center = config["machine_info"]["cost_center"].get<std::string>();
    
    return cfg;
}

// Usage
MachineConfig cfg = LoadConfig("C:\\Program Files\\LicenseChecker\\config.json");

// Send to server
json report = {
    {"machine_guid", GetMachineGUID()},
    {"department", cfg.department},
    {"site", cfg.site},
    {"cost_center", cfg.cost_center},
    {"licenseStatus", "Legitimate"}
};
```

**Advantages:**
- ✅ Đơn giản, nhanh
- ✅ Không cần query gì cả
- ✅ Offline-capable

**Disadvantages:**
- ❌ Manual per machine
- ❌ Dễ sai nếu config sai
- ❌ Khó quản lý 100+ máy

**Best for:** Tổ chức nhỏ (<50 máy)

---

### Strategy 2: Active Directory (Automatic) ⭐⭐⭐⭐⭐

**How it works:**
- Agent gửi GUID (không cần department)
- Server query AD để tìm phòng ban
- Server tự động gán department

**Server-side Python:**
```python
import ldap
from ldap import modlist as ldapmodlist

class ActiveDirectoryManager:
    def __init__(self, ad_server, ad_user, ad_password):
        self.conn = ldap.initialize(f'ldap://{ad_server}')
        self.conn.simple_bind_s(ad_user, ad_password)
        self.base_dn = "DC=corp,DC=local"
    
    def get_machine_department_from_ad(self, hostname):
        """
        Query Active Directory để lấy phòng ban của máy
        """
        search_filter = f"(&(objectClass=computer)(name={hostname}))"
        
        result = self.conn.search_s(
            self.base_dn,
            ldap.SCOPE_SUBTREE,
            search_filter,
            ['department', 'ou']
        )
        
        if result:
            dn, attrs = result[0]
            
            # Get department từ AD attributes
            if b'department' in attrs:
                department = attrs[b'department'][0].decode('utf-8')
                return department
            
            # Alternative: Extract từ DN
            # OU=Finance,OU=Departments,DC=corp,DC=local
            if dn:
                parts = dn.split(',')
                for part in parts:
                    if part.startswith('OU='):
                        ou_name = part.replace('OU=', '').strip()
                        if ou_name.lower() != 'departments':
                            return ou_name
        
        return "Unknown"
    
    def get_machine_by_guid(self, machine_guid):
        """
        Query AD bằng Machine GUID
        """
        # Convert GUID to AD format
        guid_hex = machine_guid.replace('-', '')
        search_filter = f"(objectGUID={guid_hex})"
        
        result = self.conn.search_s(
            self.base_dn,
            ldap.SCOPE_SUBTREE,
            search_filter,
            ['cn', 'department', 'mail']
        )
        
        if result:
            dn, attrs = result[0]
            hostname = attrs[b'cn'][0].decode('utf-8')
            department = attrs.get(b'department', [b'Unknown'])[0].decode('utf-8')
            
            return {
                'hostname': hostname,
                'department': department,
                'dn': dn
            }
        
        return None
```

**Agent Report (gửi lên):**
```json
{
  "machine_guid": "12345678-1234-1234-1234-123456789012",
  "hostname": "DESKTOP-ABC123",
  "primary_mac": "00:1A:2B:3C:4D:5E",
  "os_version": 10
}
```

**Server Processing:**
```python
@app.post("/api/agent/report-license")
async def report_license(data: dict):
    machine_guid = data['machine_guid']
    hostname = data['hostname']
    
    # Query AD để lấy department
    ad_manager = ActiveDirectoryManager(
        ad_server="corp-ad.local",
        ad_user="service_account@corp.local",
        ad_password="password"
    )
    
    ad_info = ad_manager.get_machine_by_guid(machine_guid)
    
    if ad_info:
        department = ad_info['department']
    else:
        department = "Unknown"
    
    # Save to database
    db.query("""
        INSERT OR REPLACE INTO machines 
        (machine_guid, hostname, department, last_check_in)
        VALUES (?, ?, ?, NOW())
    """, (machine_guid, hostname, department))
    
    return {"status": "received"}
```

**Advantages:**
- ✅ Automatic (no manual config)
- ✅ Always up-to-date (changes reflected)
- ✅ Scales to 1000+ machines
- ✅ Auditable (AD is source of truth)

**Disadvantages:**
- ❌ Requires AD connectivity
- ❌ Setup complexity
- ❌ Non-domain machines won't work

**Best for:** Enterprise với Active Directory

---

### Strategy 3: Hybrid (Agent + AD Verification) ⭐⭐⭐⭐⭐ RECOMMENDED

**How it works:**
- Agent gửi department từ config file
- Server verify với AD
- Nếu không match → log warning
- Nếu AD update → server tự động đồng bộ

**Agent Report:**
```json
{
  "machine_guid": "12345678-1234-1234-1234-123456789012",
  "hostname": "DESKTOP-ABC123",
  "department_claimed": "Finance",  // Từ config
  "primary_mac": "00:1A:2B:3C:4D:5E"
}
```

**Server Processing:**
```python
@app.post("/api/agent/report-license")
async def report_license(data: dict):
    machine_guid = data['machine_guid']
    hostname = data['hostname']
    department_claimed = data.get('department_claimed', 'Unknown')
    
    # Step 1: Check existing machine
    existing = db.query(
        "SELECT id, department FROM machines WHERE machine_guid = ?",
        (machine_guid,)
    )
    
    if existing:
        machine_id, stored_department = existing[0]
        
        # Step 2: Try to verify with AD
        ad_info = ad_manager.get_machine_by_guid(machine_guid)
        
        if ad_info:
            ad_department = ad_info['department']
            
            # Check if claimed department matches AD
            if department_claimed != ad_department:
                log_warning(f"""
                    Department mismatch for {hostname}:
                    Claimed: {department_claimed}
                    AD Says: {ad_department}
                    Using: {ad_department}
                """)
                department = ad_department
            else:
                department = ad_department
        else:
            # AD not available, trust agent claim
            department = department_claimed
        
        # Update if changed
        if department != stored_department:
            db.query(
                "UPDATE machines SET department = ? WHERE id = ?",
                (department, machine_id)
            )
    else:
        # New machine - determine department
        ad_info = ad_manager.get_machine_by_guid(machine_guid)
        
        if ad_info:
            department = ad_info['department']
        else:
            # AD not found, use agent's claim
            department = department_claimed
        
        # Create new machine
        machine_id = create_new_machine({
            'machine_guid': machine_guid,
            'hostname': hostname,
            'department': department
        })
    
    # Continue with license update...
    update_license(machine_id, data)
    
    return {"status": "received", "machine_id": machine_id}
```

**Advantages:**
- ✅ Works with or without AD
- ✅ Self-healing (auto-correct from AD)
- ✅ Auditable
- ✅ Detects misconfigurations

**Disadvantages:**
- ❌ Slightly more complex
- ❌ Depends on AD availability

**Best for:** Mixed environments (domain + non-domain machines)

---

### Strategy 4: Manual Dashboard Assignment ⭐⭐

**How it works:**
- Agent gửi GUID (không gửi department)
- Server lưu máy với department = "Unassigned"
- Admin thủ công assign department từ dashboard

**Agent Report (simple):**
```json
{
  "machine_guid": "12345678-1234-1234-1234-123456789012",
  "hostname": "DESKTOP-ABC123",
  "primary_mac": "00:1A:2B:3C:4D:5E",
  "os_version": 10
}
```

**Server (auto-assign to Unassigned):**
```python
@app.post("/api/agent/report-license")
async def report_license(data: dict):
    machine_guid = data['machine_guid']
    
    # Check if exists
    existing = db.query(
        "SELECT id FROM machines WHERE machine_guid = ?",
        (machine_guid,)
    )
    
    if not existing:
        # Create with unassigned department
        db.query("""
            INSERT INTO machines 
            (machine_guid, hostname, department_id, last_check_in)
            VALUES (?, ?, 
                (SELECT id FROM departments WHERE name = 'Unassigned'),
                NOW())
        """, (machine_guid, data['hostname']))
    
    return {"status": "received"}
```

**Dashboard API (Admin assign):**
```python
@app.put("/api/machines/{machine_id}/assign-department")
async def assign_department(machine_id: int, department: str):
    """
    Admin manually assign machine to department
    """
    db.query(
        """UPDATE machines 
           SET department_id = 
           (SELECT id FROM departments WHERE name = ?)
           WHERE id = ?""",
        (department, machine_id)
    )
    
    # Log action
    db.query("""
        INSERT INTO audit_log (action, resource_type, resource_id, details)
        VALUES ('ASSIGN_DEPARTMENT', 'machine', ?, ?)
    """, (machine_id, f"Assigned to: {department}"))
    
    return {"status": "assigned"}
```

**Dashboard UI:**
```html
<!-- Machines Tab: Unassigned Section -->
<div id="unassigned-machines">
    <h3>Unassigned Machines (10)</h3>
    <table>
        <tr>
            <td>LAPTOP-NEW-001</td>
            <td>Unassigned</td>
            <td>
                <select id="dept-select">
                    <option>-- Select Department --</option>
                    <option>Finance</option>
                    <option>IT</option>
                    <option>HR</option>
                </select>
                <button onclick="assignDepartment(1, this.previousElementSibling.value)">
                    Assign
                </button>
            </td>
        </tr>
    </table>
</div>
```

**Advantages:**
- ✅ Simple (no config, no AD dependency)
- ✅ Flexible (can change anytime)
- ✅ Works offline

**Disadvantages:**
- ❌ Manual work
- ❌ Easy to forget
- ❌ Not scalable (100+ machines)

**Best for:** Very small organization or testing

---

## Comparison Table

| Strategy | Complexity | Accuracy | Scalability | Maintenance | Best For |
|----------|-----------|----------|-------------|-------------|----------|
| **Agent Config** | ⭐ Low | ⭐⭐ Medium | ⭐ Poor | Manual per machine | <50 machines |
| **Active Directory** | ⭐⭐⭐⭐ High | ⭐⭐⭐⭐⭐ Very High | ⭐⭐⭐⭐⭐ Excellent | Auto | Enterprise with AD |
| **Hybrid (Recommended)** | ⭐⭐⭐ Medium | ⭐⭐⭐⭐⭐ Very High | ⭐⭐⭐⭐ Good | Minimal | Mixed environment |
| **Manual Dashboard** | ⭐ Low | ⭐⭐ Medium | ⭐ Poor | Manual per machine | Testing/Small |

---

## RECOMMENDED APPROACH: Hybrid Strategy

**Combines best of both worlds:**

### Agent Side (C++):
```cpp
struct MachineReport {
    std::string machine_guid;
    std::string hostname;
    std::string primary_mac;
    std::string department_claimed;  // From config
    int os_version;
    std::string license_status;
};

MachineReport CreateReport() {
    MachineReport report;
    report.machine_guid = GetMachineGUID();
    report.hostname = GetComputerName();
    report.primary_mac = GetPrimaryMAC();
    
    // Read from config file
    MachineConfig cfg = LoadConfig("config.json");
    report.department_claimed = cfg.department;
    
    report.os_version = GetWindowsVersion();
    report.license_status = "Legitimate";
    
    return report;
}
```

### Agent Config File:
```json
{
  "server": {
    "url": "http://server:8000",
    "api_key": "key-12345"
  },
  "machine_info": {
    "hostname": "auto",
    "department": "Finance",
    "site": "Hanoi",
    "cost_center": "CC-001"
  }
}
```

### Server Side (Python):
```python
from ldap_manager import ActiveDirectoryManager

ad_manager = ActiveDirectoryManager(
    ad_server="corp-ad.local",
    ad_user="service_account@corp.local",
    ad_password="password"
)

@app.post("/api/agent/report-license")
async def report_license(data: dict):
    """
    Hybrid approach: Agent provides department, server verifies with AD
    """
    machine_guid = data['machine_guid']
    hostname = data['hostname']
    department_claimed = data.get('department_claimed', 'Unknown')
    
    # Try AD first (source of truth)
    ad_info = ad_manager.get_machine_by_guid(machine_guid)
    
    if ad_info:
        ad_department = ad_info['department']
        
        # Log if mismatch
        if department_claimed != ad_department:
            log_warning(f"""
                Department mismatch:
                Agent claimed: {department_claimed}
                AD actual: {ad_department}
                Using AD value
            """)
        
        department = ad_department
    else:
        # AD unavailable, trust agent
        department = department_claimed
    
    # Find or create machine
    machine = db.query(
        "SELECT id FROM machines WHERE machine_guid = ?",
        (machine_guid,)
    )
    
    if machine:
        machine_id = machine[0]
        # Update department if changed
        db.query(
            "UPDATE machines SET department = ? WHERE id = ?",
            (department, machine_id)
        )
    else:
        # Create new
        db.query("""
            INSERT INTO machines (machine_guid, hostname, department)
            VALUES (?, ?, ?)
        """, (machine_guid, hostname, department))
        machine_id = db.lastrowid
    
    # Update license
    update_license(machine_id, data)
    
    return {
        "status": "received",
        "machine_id": machine_id,
        "department_assigned": department
    }
```

### Server Database Schema:
```sql
ALTER TABLE machines ADD COLUMN department_claimed VARCHAR(255);
ALTER TABLE machines ADD COLUMN department_source VARCHAR(50);  -- 'AD', 'CONFIG', 'MANUAL'
ALTER TABLE machines ADD COLUMN last_ad_sync TIMESTAMP;

-- Track department changes
CREATE TABLE department_changes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    machine_id INTEGER NOT NULL,
    old_department VARCHAR(255),
    new_department VARCHAR(255),
    source VARCHAR(50),  -- 'AD_SYNC', 'MANUAL', 'CONFIG'
    changed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (machine_id) REFERENCES machines(id)
);
```

### Dashboard - Unassigned Machines:
```html
<div id="department-verification">
    <h3>Verify Departments</h3>
    <table>
        <tr>
            <th>Machine</th>
            <th>Agent Says</th>
            <th>AD Says</th>
            <th>Status</th>
            <th>Action</th>
        </tr>
        <tr>
            <td>LAPTOP-001</td>
            <td>Finance</td>
            <td>HR</td>
            <td>⚠️ MISMATCH</td>
            <td>
                <button>Use AD (HR)</button>
                <button>Use Agent (Finance)</button>
            </td>
        </tr>
    </table>
</div>
```

---

## Implementation Checklist

### For Agent Config Approach:
- [ ] Create config.json template
- [ ] Document department values
- [ ] Generate config per machine
- [ ] Deploy config with agent installer

### For Active Directory Approach:
- [ ] Set up LDAP connection to AD
- [ ] Test AD queries
- [ ] Create service account
- [ ] Handle AD unavailability (fallback)
- [ ] Test with sample machines

### For Hybrid Approach (Recommended):
- [ ] Agent reads department from config
- [ ] Agent sends in report
- [ ] Server queries AD
- [ ] Server logs mismatches
- [ ] Dashboard shows mismatches
- [ ] Admin can verify/override
- [ ] Create audit trail

### Dashboard Features:
- [ ] Show "Unassigned" machines
- [ ] Show "Mismatched" departments
- [ ] Manual assignment UI
- [ ] Department verification report
- [ ] Change history tracking

---

## Real-World Scenarios

### Scenario 1: New Machine
```
1. Agent boots, reads config.json: department = "Finance"
2. Agent sends GUID + "Finance" claim to server
3. Server queries AD with GUID
4. AD returns: department = "Finance" ✅ Match!
5. Server stores: machine linked to Finance
```

### Scenario 2: Misassigned Machine
```
1. Admin put wrong department in config: "IT"
2. Agent sends GUID + "IT" claim
3. Server queries AD: returns "Finance"
4. Server detects MISMATCH, logs warning
5. Dashboard shows: ⚠️ "IT" vs "Finance"
6. Admin verifies and fixes config
```

### Scenario 3: AD Unavailable
```
1. Agent sends GUID + "Finance" claim
2. Server tries AD query → Connection failed
3. Server falls back to agent's claim: "Finance"
4. Server stores: Finance (from agent)
5. Later when AD available → Re-sync with AD
```

### Scenario 4: Machine Moved
```
1. User moves machine from Finance to HR
2. AD updated
3. Agent still sends old config: "Finance"
4. Server queries AD: returns "HR"
5. Server detects change, updates to "HR"
6. Dashboard shows change history
```

---

## Summary

**Best Practice for your system:**

✅ **Use Hybrid Approach:**
1. Agent sends department from config file
2. Server verifies with Active Directory
3. Server auto-corrects if mismatch
4. Dashboard shows verification status
5. Admin can manually override if needed

**Result:**
- ✓ Simple for small orgs
- ✓ Scales to enterprise
- ✓ Self-healing from AD
- ✓ Auditable
- ✓ Detects misconfigurations
- ✓ Works with/without AD

This gives you the best of all worlds! 🎯
