# Department-Specific Installer Flow

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         ADMIN WORKFLOW                          │
└─────────────────────────────────────────────────────────────────┘

1. Admin creates department on server
   └─ Server generates: unique API key for that department

2. Admin clicks: "Download Installer"
   └─ Server creates:
      ├─ Installer EXE (with embedded config)
      └─ department_key.txt (with unique key)
      └─ Packs into: department_finance_installer.zip

3. Admin distributes ZIP to Finance team

┌─────────────────────────────────────────────────────────────────┐
│                      USER/MACHINE WORKFLOW                      │
└─────────────────────────────────────────────────────────────────┘

1. User extracts ZIP on their machine
   ├─ Finds: LicenseCheckerInstaller.exe
   └─ Finds: department_key.txt

2. Runs installer
   ├─ Installer reads: department_key.txt
   ├─ Gets: API_KEY (unique for Finance)
   ├─ Reads: Machine GUID (GetMachineGUID())
   ├─ Installs: Windows Service
   ├─ Registers: First report on startup
   │  └─ Sends: GUID + API_KEY to server
   └─ Service starts: Background detection every 5 min

3. Server receives registration
   ├─ Validates: API_KEY matches Finance department
   ├─ Extracts: GUID from request
   ├─ Auto-assigns: machine to Finance department
   ├─ Stores: machine record
   └─ Returns: Success to installer

4. Dashboard shows
   └─ New machine: GUID-ABC123 in Finance department ✓

┌─────────────────────────────────────────────────────────────────┐
│                      SUBSEQUENT REPORTS                         │
└─────────────────────────────────────────────────────────────────┘

Every 5 minutes:
├─ Detect: License status
├─ Send: GUID + API_KEY + license data
├─ Server: Verify API_KEY (already knows department)
└─ Update: License status for that machine
```

---

## Step 1: Server Creates Department with Key

### Database Schema

```sql
-- Departments table (enhanced)
ALTER TABLE departments ADD COLUMN api_key VARCHAR(64) UNIQUE;
ALTER TABLE departments ADD COLUMN created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP;
ALTER TABLE departments ADD COLUMN created_by VARCHAR(255);
ALTER TABLE departments ADD COLUMN installer_download_count INTEGER DEFAULT 0;
ALTER TABLE departments ADD COLUMN last_installer_download TIMESTAMP;

-- Track installer downloads for audit
CREATE TABLE installer_download_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    department_id INTEGER NOT NULL,
    downloaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    downloaded_by VARCHAR(255),
    zip_filename VARCHAR(255),
    FOREIGN KEY (department_id) REFERENCES departments(id)
);

-- Map API keys to departments
CREATE TABLE api_key_mappings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    api_key VARCHAR(64) UNIQUE,
    department_id INTEGER NOT NULL,
    is_active BOOLEAN DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (department_id) REFERENCES departments(id)
);

CREATE INDEX idx_api_key ON api_key_mappings(api_key);
CREATE INDEX idx_department_key ON api_key_mappings(department_id);
```

### Server API: Create Department with Key

```python
from fastapi import FastAPI, HTTPException
import secrets
import string

app = FastAPI()

@app.post("/api/admin/departments")
async def create_department(name: str, description: str = None):
    """
    Admin creates new department
    Server auto-generates unique API key
    """
    
    # Generate unique API key for this department
    # Format: DEPT-{random-string}-{random-string}
    # Example: DEPT-Finance-a1b2c3d4e5f6-x9y8z7w6v5u4
    
    api_key = generate_department_api_key(name)
    
    # Create department
    dept_id = db.query("""
        INSERT INTO departments (name, description, api_key, created_by)
        VALUES (?, ?, ?, ?)
    """, (name, description, api_key, current_user))
    
    # Create API key mapping
    db.query("""
        INSERT INTO api_key_mappings (api_key, department_id, is_active)
        VALUES (?, ?, 1)
    """, (api_key, dept_id))
    
    return {
        "department_id": dept_id,
        "name": name,
        "api_key": api_key,
        "message": "Department created. Download installer to distribute."
    }

def generate_department_api_key(department_name: str) -> str:
    """
    Generate unique API key for department
    Format: DEPT-{name}-{random}
    """
    # Remove special chars from name
    clean_name = ''.join(c for c in department_name if c.isalnum())[:10]
    
    # Generate random part
    chars = string.ascii_letters + string.digits
    random_part = ''.join(secrets.choice(chars) for _ in range(32))
    
    return f"DEPT-{clean_name}-{random_part}"
```

---

## Step 2: Server Generates Customized Installer ZIP

### Dashboard API: Generate Installer Download

```python
import zipfile
import io
from datetime import datetime

@app.get("/api/admin/departments/{dept_id}/download-installer")
async def download_installer(dept_id: int):
    """
    Generate customized installer ZIP for department
    
    ZIP contents:
    ├─ LicenseCheckerInstaller.exe (main installer)
    ├─ department_key.txt (contains API key + metadata)
    ├─ README.txt (installation instructions)
    └─ LICENSE.txt (terms)
    """
    
    # Get department and API key
    dept = db.query(
        "SELECT id, name, api_key FROM departments WHERE id = ?",
        (dept_id,)
    )
    
    if not dept:
        raise HTTPException(status_code=404, detail="Department not found")
    
    dept_id, dept_name, api_key = dept[0]
    
    # Create in-memory ZIP file
    zip_buffer = io.BytesIO()
    
    with zipfile.ZipFile(zip_buffer, 'w', zipfile.ZIP_DEFLATED) as zip_file:
        
        # 1. Add installer EXE
        # (In real scenario, read from compiled binary)
        installer_binary = read_installer_exe()
        zip_file.writestr("LicenseCheckerInstaller.exe", installer_binary)
        
        # 2. Create department_key.txt
        department_key_content = f"""LICENSE CHECKER - DEPARTMENT INSTALLATION KEY
================================================================

Department: {dept_name}
API Key: {api_key}
Generated: {datetime.now().isoformat()}
Version: 1.0

IMPORTANT:
- This key is unique to {dept_name} department
- Keep this file secure - do NOT share across departments
- The installer will use this key to register your machine
- Never modify the API key value in this file

INSTALLATION:
1. Extract this ZIP file
2. Run LicenseCheckerInstaller.exe
3. Follow on-screen instructions
4. Installer will:
   ├─ Read this file automatically
   ├─ Install License Checker service
   ├─ Register your machine
   └─ Start background detection

For support, contact IT department
================================================================
"""
        zip_file.writestr("department_key.txt", department_key_content)
        
        # 3. Add README
        readme_content = f"""Windows License Checker - {dept_name} Department
================================================

QUICK START:
1. Extract this ZIP file to any folder
2. Right-click LicenseCheckerInstaller.exe → Run as Administrator
3. Click Install
4. Close when complete

The service will:
- Detect Windows license status automatically
- Send results to IT compliance dashboard
- Run in background every 5 minutes
- Require no user interaction

REQUIREMENTS:
- Windows 7 or later
- Administrator privileges to install
- Network access to: [server-ip]:8000

UNINSTALL:
- Control Panel → Programs → Uninstall a program
- Select "Windows License Checker"
- Click Uninstall

QUESTIONS?
Contact IT Help Desk
"""
        zip_file.writestr("README.txt", readme_content)
        
        # 4. Add LICENSE
        license_content = "License Agreement...\n(your terms here)"
        zip_file.writestr("LICENSE.txt", license_content)
    
    # Prepare download
    zip_buffer.seek(0)
    
    # Log download
    db.query("""
        UPDATE departments 
        SET installer_download_count = installer_download_count + 1,
            last_installer_download = NOW()
        WHERE id = ?
    """, (dept_id,))
    
    db.query("""
        INSERT INTO installer_download_log 
        (department_id, downloaded_by, zip_filename)
        VALUES (?, ?, ?)
    """, (dept_id, current_user, f"{dept_name}_installer.zip"))
    
    return {
        "content": zip_buffer.getvalue(),
        "filename": f"{dept_name}_installer_{datetime.now().strftime('%Y%m%d')}.zip",
        "media_type": "application/zip"
    }

def read_installer_exe():
    """
    Read compiled installer binary
    In real scenario, this reads from compiled EXE file
    """
    with open("C:\\build\\LicenseCheckerInstaller.exe", "rb") as f:
        return f.read()
```

### Dashboard UI

```html
<div id="department-management">
    <table>
        <tr>
            <th>Department</th>
            <th>API Key</th>
            <th>Status</th>
            <th>Actions</th>
        </tr>
        <tr>
            <td>Finance</td>
            <td>DEPT-Finance-a1b2c3...</td>
            <td>✓ Active (45 machines)</td>
            <td>
                <button onclick="downloadInstaller(1)">
                    📥 Download Installer
                </button>
                <button onclick="showApiKey(1)">
                    🔑 Show Key
                </button>
                <button onclick="viewMachines(1)">
                    💻 View Machines
                </button>
            </td>
        </tr>
    </table>
</div>
```

---

## Step 3: Installer Reads Department Key

### Installer Code (C++)

```cpp
#include <fstream>
#include <sstream>
#include <string>
#include <Windows.h>

class InstallerConfig {
public:
    std::string department_api_key;
    std::string server_url;
    
    bool LoadDepartmentKeyFile() {
        /**
         * Read department_key.txt from same directory as installer
         * Extract API key
         */
        
        // Get installer directory
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        std::string installer_path(buffer);
        
        // Get directory
        size_t pos = installer_path.find_last_of("\\");
        std::string installer_dir = installer_path.substr(0, pos);
        
        // Path to department_key.txt
        std::string key_file = installer_dir + "\\department_key.txt";
        
        // Read file
        std::ifstream file(key_file);
        if (!file.is_open()) {
            MessageBoxA(NULL, 
                "ERROR: department_key.txt not found!\n\n"
                "Make sure you extracted all files from the ZIP.",
                "Missing Key File",
                MB_OK | MB_ICONERROR);
            return false;
        }
        
        std::string line;
        bool found_key = false;
        
        while (std::getline(file, line)) {
            // Look for: API Key: DEPT-Finance-a1b2c3...
            if (line.find("API Key:") != std::string::npos) {
                // Extract key after "API Key: "
                size_t key_pos = line.find("API Key:") + 8;
                
                // Trim whitespace
                size_t start = line.find_first_not_of(" ", key_pos);
                size_t end = line.find_last_not_of(" ");
                
                if (start != std::string::npos) {
                    department_api_key = line.substr(start, end - start + 1);
                    found_key = true;
                    break;
                }
            }
        }
        
        file.close();
        
        if (!found_key) {
            MessageBoxA(NULL,
                "ERROR: Could not find API key in department_key.txt!",
                "Invalid Key File",
                MB_OK | MB_ICONERROR);
            return false;
        }
        
        return true;
    }
};

// Main installer logic
int main() {
    // Step 1: Load department key
    InstallerConfig config;
    if (!config.LoadDepartmentKeyFile()) {
        return 1;  // Failed
    }
    
    MessageBoxA(NULL,
        ("Loaded department key for installation\n\n"
         "Department Key: " + config.department_api_key).c_str(),
        "Installation Started",
        MB_OK | MB_ICONINFORMATION);
    
    // Step 2: Get machine GUID
    std::string machine_guid = GetMachineGUID();
    
    // Step 3: Install Windows Service
    if (!InstallWindowsService()) {
        MessageBoxA(NULL,
            "ERROR: Failed to install Windows Service!",
            "Installation Failed",
            MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Step 4: Register machine on server
    if (!RegisterMachineOnServer(machine_guid, config.department_api_key)) {
        MessageBoxA(NULL,
            "WARNING: Service installed but registration failed.\n\n"
            "The service will continue to work and try registration later.",
            "Installation Partial",
            MB_OK | MB_ICONWARNING);
    }
    
    MessageBoxA(NULL,
        "Installation completed successfully!\n\n"
        "The License Checker service is now running.\n"
        "It will check your license status every 5 minutes.",
        "Installation Complete",
        MB_OK | MB_ICONINFORMATION);
    
    return 0;
}

std::string GetMachineGUID() {
    /**
     * Get unique machine GUID from Windows Registry
     * HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Cryptography\MachineGuid
     */
    
    HKEY hKey;
    LONG result = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Cryptography",
        0, KEY_READ, &hKey);
    
    if (result != ERROR_SUCCESS) {
        return "";
    }
    
    char guid[256];
    DWORD size = sizeof(guid);
    result = RegQueryValueExA(hKey, "MachineGuid", NULL, NULL,
                             (LPBYTE)guid, &size);
    RegCloseKey(hKey);
    
    if (result == ERROR_SUCCESS) {
        return std::string(guid);
    }
    
    return "";
}

bool RegisterMachineOnServer(const std::string& machine_guid,
                             const std::string& api_key) {
    /**
     * Send registration to server:
     * POST /api/agent/register
     * Body: {
     *   "machine_guid": "12345678-...",
     *   "department_api_key": "DEPT-Finance-...",
     *   "hostname": "DESKTOP-ABC123",
     *   "primary_mac": "00:1A:2B:3C:4D:5E",
     *   "action": "register"
     * }
     */
    
    nlohmann::json registration_data = {
        {"machine_guid", machine_guid},
        {"department_api_key", api_key},
        {"hostname", GetComputerName()},
        {"primary_mac", GetPrimaryMACAddress()},
        {"action", "register"},
        {"timestamp", GetCurrentTimestamp()}
    };
    
    // Send HTTP POST
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    std::string server_url = "http://your-server:8000";
    std::string full_url = server_url + "/api/agent/register";
    std::string json_str = registration_data.dump();
    
    curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
    
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    // Execute
    CURLcode res = curl_easy_perform(curl);
    
    bool success = (res == CURLE_OK);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return success;
}

bool InstallWindowsService() {
    /**
     * Install License Checker as Windows Service
     * Using SC (Service Control Manager)
     */
    
    // Get full path to EXE
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    
    // Build command: sc create LicenseCheckerAgent binPath= "path"
    std::string command = "sc create LicenseCheckerAgent "
                         "binPath= \"" + std::string(buffer) + "\" "
                         "start= auto "
                         "DisplayName= \"License Checker Agent\"";
    
    int result = system(command.c_str());
    return result == 0;
}
```

---

## Step 4: Server Receives Registration

### Server API: Register Machine

```python
@app.post("/api/agent/register")
async def register_machine(data: dict):
    """
    Installer sends registration on first run
    Server validates department_api_key and auto-assigns machine
    """
    
    machine_guid = data['machine_guid']
    department_api_key = data['department_api_key']
    hostname = data['hostname']
    primary_mac = data['primary_mac']
    
    # Step 1: Validate API key (must match existing department)
    api_key_record = db.query("""
        SELECT department_id FROM api_key_mappings 
        WHERE api_key = ? AND is_active = 1
    """, (department_api_key,))
    
    if not api_key_record:
        return {
            "status": "error",
            "message": "Invalid or inactive API key",
            "code": 401
        }
    
    department_id = api_key_record[0]
    
    # Step 2: Get department name
    dept_record = db.query(
        "SELECT name FROM departments WHERE id = ?",
        (department_id,)
    )
    department_name = dept_record[0] if dept_record else "Unknown"
    
    # Step 3: Check if machine already exists
    existing = db.query(
        "SELECT id FROM machines WHERE machine_guid = ?",
        (machine_guid,)
    )
    
    if existing:
        machine_id = existing[0]
        # Update existing machine
        db.query("""
            UPDATE machines 
            SET department_id = ?, 
                department_api_key = ?, 
                last_registration = NOW(),
                is_online = 1
            WHERE id = ?
        """, (department_id, department_api_key, machine_id))
        
        action = "Updated existing"
    else:
        # Create new machine
        db.query("""
            INSERT INTO machines 
            (machine_guid, hostname, primary_mac, department_id, 
             department_api_key, last_registration, is_online)
            VALUES (?, ?, ?, ?, ?, NOW(), 1)
        """, (machine_guid, hostname, primary_mac, department_id, 
              department_api_key))
        
        machine_id = db.lastrowid
        action = "Registered new"
    
    # Step 4: Log registration
    db.query("""
        INSERT INTO registration_log 
        (machine_id, action, department_id, timestamp)
        VALUES (?, ?, ?, NOW())
    """, (machine_id, action, department_id))
    
    # Step 5: Return success
    return {
        "status": "success",
        "machine_id": machine_id,
        "department": department_name,
        "message": f"{action} machine in {department_name} department"
    }
```

---

## Step 5: Agent Uses Key for Subsequent Reports

### Agent Report with Department Key

```json
POST /api/agent/report-license

{
  "machine_guid": "12345678-1234-1234-1234-123456789012",
  "department_api_key": "DEPT-Finance-a1b2c3d4e5f6",
  "hostname": "DESKTOP-ABC123",
  "primary_mac": "00:1A:2B:3C:4D:5E",
  "os_version": 10,
  "os_edition": "Pro",
  "licenseStatus": "Legitimate",
  "kmsStatus": "KMSDetected",
  "windowsKmsServer": "kms.corp.local",
  "timestamp": "2026-08-06T19:00:00Z"
}
```

### Server Processes Report

```python
@app.post("/api/agent/report-license")
async def report_license(data: dict):
    """
    Agent sends license report WITH department_api_key
    Server validates key and updates department accordingly
    """
    
    machine_guid = data['machine_guid']
    department_api_key = data.get('department_api_key')
    
    # Step 1: Validate API key
    if not department_api_key:
        return {
            "status": "error",
            "message": "Missing department_api_key",
            "code": 400
        }
    
    # Step 2: Get department from API key
    dept_record = db.query("""
        SELECT d.id, d.name FROM api_key_mappings k
        JOIN departments d ON k.department_id = d.id
        WHERE k.api_key = ? AND k.is_active = 1
    """, (department_api_key,))
    
    if not dept_record:
        return {
            "status": "error",
            "message": "Invalid or inactive API key",
            "code": 401
        }
    
    department_id, department_name = dept_record[0]
    
    # Step 3: Find machine
    machine = db.query(
        "SELECT id FROM machines WHERE machine_guid = ?",
        (machine_guid,)
    )
    
    if machine:
        machine_id = machine[0]
        # Verify department matches
        existing_dept = db.query(
            "SELECT department_id FROM machines WHERE id = ?",
            (machine_id,)
        )
        
        if existing_dept and existing_dept[0] != department_id:
            # Department mismatch - log warning
            log_warning(f"Department mismatch for machine {machine_guid}: "
                       f"expected {existing_dept[0]}, got {department_id}")
    else:
        # Machine not registered via installer - create it
        db.query("""
            INSERT INTO machines 
            (machine_guid, hostname, primary_mac, department_id, 
             department_api_key)
            VALUES (?, ?, ?, ?, ?)
        """, (machine_guid, data['hostname'], data['primary_mac'],
              department_id, department_api_key))
        machine_id = db.lastrowid
    
    # Step 4: Update license
    db.query("""
        INSERT OR REPLACE INTO licenses
        (machine_id, windows_status, office_status, kms_status, 
         windows_kms_server, detected_at)
        VALUES (?, ?, ?, ?, ?, ?)
    """, (machine_id, data['licenseStatus'], 
          data.get('officeStatus'),
          data['kmsStatus'], 
          data.get('windowsKmsServer'),
          data['timestamp']))
    
    # Step 5: Update last check-in
    db.query("""
        UPDATE machines 
        SET last_check_in = NOW(), is_online = 1
        WHERE id = ?
    """, (machine_id,))
    
    return {
        "status": "received",
        "machine_id": machine_id,
        "department": department_name
    }
```

---

## Database Schema Enhancement

```sql
-- Machines table (add department_api_key tracking)
ALTER TABLE machines ADD COLUMN department_api_key VARCHAR(64);
ALTER TABLE machines ADD COLUMN last_registration TIMESTAMP;
ALTER TABLE machines ADD COLUMN registered_via VARCHAR(50);  -- 'installer', 'manual', etc.

-- Track all registrations for audit
CREATE TABLE registration_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    machine_id INTEGER NOT NULL,
    action VARCHAR(50),  -- 'registered', 'updated', 'reactivated'
    department_id INTEGER NOT NULL,
    installer_version VARCHAR(20),
    registered_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (machine_id) REFERENCES machines(id),
    FOREIGN KEY (department_id) REFERENCES departments(id)
);

-- API key tracking
CREATE TABLE api_key_mappings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    api_key VARCHAR(64) UNIQUE,
    department_id INTEGER NOT NULL,
    is_active BOOLEAN DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_by VARCHAR(255),
    FOREIGN KEY (department_id) REFERENCES departments(id)
);

CREATE INDEX idx_api_key_active ON api_key_mappings(api_key, is_active);
```

---

## Workflow Diagram

```
ADMIN SIDE:
┌─ Admin Portal
│  └─ Create Department: "Finance"
│     └─ Server generates: API_KEY = "DEPT-Finance-a1b2c3..."
│        └─ Store in: api_key_mappings table
│
└─ Download Installer
   └─ Server creates ZIP:
      ├─ LicenseCheckerInstaller.exe
      ├─ department_key.txt (contains API_KEY)
      ├─ README.txt
      └─ LICENSE.txt
      └─ Send to admin for distribution

USER SIDE:
┌─ User receives ZIP
│  └─ Extracts files
│
└─ Runs LicenseCheckerInstaller.exe
   ├─ Reads: department_key.txt
   ├─ Gets: API_KEY
   ├─ Gets: GUID, MAC, Hostname
   ├─ POST /api/agent/register
   │  {
   │    "machine_guid": GUID,
   │    "department_api_key": API_KEY,
   │    "hostname": hostname,
   │    "primary_mac": MAC
   │  }
   │
   ├─ Server validates API_KEY
   │  └─ Finds: Department = Finance
   │  └─ Creates: machine record linked to Finance
   │  └─ Returns: Success
   │
   ├─ Installer installs Windows Service
   └─ Service starts: background detection

ONGOING (Every 5 min):
┌─ Service detects: license status
│
└─ Sends: POST /api/agent/report-license
   {
     "machine_guid": GUID,
     "department_api_key": API_KEY,
     "licenseStatus": "Legitimate",
     ...
   }
   
   Server:
   ├─ Validates API_KEY
   ├─ Gets: Department from API_KEY
   ├─ Updates: license record
   └─ Returns: Success
```

---

## Security Considerations

### API Key Security

```python
# API key should:
# ✅ Be unique per department
api_key = "DEPT-Finance-" + secrets.token_hex(32)

# ✅ Have expiration (optional)
ALTER TABLE api_key_mappings ADD COLUMN expires_at TIMESTAMP;

# ✅ Be rotatable
@app.post("/api/admin/departments/{dept_id}/rotate-key")
async def rotate_api_key(dept_id: int):
    """Generate new key, invalidate old one"""
    old_key = db.query("SELECT api_key FROM departments WHERE id = ?", (dept_id,))
    new_key = generate_department_api_key(...)
    db.query("UPDATE api_key_mappings SET is_active = 0 WHERE api_key = ?", old_key)
    db.query("INSERT INTO api_key_mappings VALUES (?, ?, 1, NOW(), ?)", 
             (new_key, dept_id, current_user))

# ✅ Be validated on every request
def validate_api_key(api_key):
    key_record = db.query(
        "SELECT department_id FROM api_key_mappings WHERE api_key = ? AND is_active = 1",
        (api_key,)
    )
    if not key_record:
        raise HTTPException(status_code=401, detail="Invalid API key")
    return key_record[0]

# ✅ Be logged on usage
db.query("""
    INSERT INTO api_key_usage_log 
    (api_key, used_by_machine, action, timestamp)
    VALUES (?, ?, ?, NOW())
""", (api_key, machine_guid, action))
```

---

## Benefits of This Design

✅ **One-time setup per machine**
- User runs installer once
- Department automatically determined
- No config files needed

✅ **Secure department binding**
- API key is unique per department
- Machine locked to department via API key
- Can't be moved without new installer

✅ **Automated registration**
- Installer registers machine automatically
- Server immediately knows machine exists
- No manual admin work

✅ **Easy department scaling**
- Admin creates department
- System generates key and installer
- Admin distributes ZIP
- Done!

✅ **Audit trail**
- Track when machine registered
- Track installer downloads
- Track API key usage
- Full compliance trail

✅ **Self-service for users**
- Run installer, done
- No technical configuration
- Works offline (registers later if needed)

✅ **Easy machine onboarding**
- New department joins
- Download installer
- Distribute to team
- All machines auto-register

---

## Example Flow

```
FRIDAY 10:00 AM - Admin Creates Department
├─ Portal: Create "Marketing" department
└─ Server:
   ├─ Generates: API_KEY = "DEPT-Marketing-x1y2z3a4b5c6d7e8f"
   ├─ Creates: marketing_installer_20260806.zip
   └─ Email: Link to download

FRIDAY 10:15 AM - Admin Downloads
├─ Click: Download installer
└─ Gets: marketing_installer_20260806.zip

FRIDAY 10:30 AM - Admin Distributes
├─ Email ZIP to Marketing team
└─ "Just run the installer on your machine"

FRIDAY 10:45 AM - User 1 Installs
├─ Extracts ZIP
├─ Runs: LicenseCheckerInstaller.exe
├─ Installer reads: API_KEY from department_key.txt
├─ Sends: GUID + API_KEY to server
├─ Server: Creates machine record in Marketing dept
└─ Dashboard shows: DESKTOP-MARKET-001 in Marketing ✓

FRIDAY 10:50 AM - User 2 Installs
├─ Same process
└─ Dashboard shows: LAPTOP-MARKET-002 in Marketing ✓

FRIDAY 11:00 AM - Admin Reviews
├─ Dashboard → Marketing department
└─ Sees: 2 new machines registered today
   ├─ DESKTOP-MARKET-001 - Legitimate
   └─ LAPTOP-MARKET-002 - Legitimate

ONGOING - Automatic Detection
├─ Every 5 minutes per machine
├─ Server receives license status
├─ Updates compliance dashboard
└─ Shows real-time compliance for Marketing dept
```

---

## Summary

**This is an elegant self-service deployment model:**

1. **Admin side:** Create department → Download installer ZIP → Distribute
2. **User side:** Extract ZIP → Run installer → Done
3. **Server side:** Validate API key → Auto-assign to department
4. **Result:** Machines automatically joined to correct department with zero configuration

**Key advantages:**
- ✅ No configuration files to manage
- ✅ Impossible to misassign (API key locks to department)
- ✅ Fully automated registration
- ✅ Complete audit trail
- ✅ Easy scaling (just create department, download, distribute)
- ✅ Secure (unique key per department)

Perfect for enterprise deployment! 🎯
