# Agent → Server Protocol (THẬT — theo code, không phải mock)

> Nguồn: [src/agent/ServerReporter.cpp](src/agent/ServerReporter.cpp), [src/agent/ServerReporter.h](src/agent/ServerReporter.h), [src/agent/LicenseDetectionWorker.cpp](src/agent/LicenseDetectionWorker.cpp)
>
> **Lưu ý quan trọng:** File `AGENT_SERVER_INTEGRATION.md` và server ở `server/app/main.py` là **mock/test server nội bộ** của project (dùng schema `server_url`/`api_key`, payload nhiều field: hostname, department, os_version, kmsStatus...). Server thật của **BE team (VNPT)** dùng payload tối giản, khác hẳn — mô tả dưới đây.

---

## 1. Transport

- Dùng **WinHTTP** (Windows native), không dùng libcurl
- HTTP hoặc HTTPS — tự nhận diện từ scheme trong `server_url`
- Ép dùng **TLS 1.1/1.2** (để tương thích Win7 khi gọi HTTPS tới server hiện đại, ví dụ `checker.vnpt.vn` — nếu không ép, Win7 mặc định SSL3/TLS1.0 sẽ bị lỗi handshake `ERROR_WINHTTP_SECURE_FAILURE`)
- Timeout: connect/send = 5s, receive = 15s
- **Không có retry** trong `SendReport()` — gửi 1 lần/chu kỳ detect; nếu fail chỉ log lỗi, lần thử tiếp theo là chu kỳ kế tiếp (mặc định 5 phút)

## 2. Endpoint

```
POST {server_url}/api/agent/report-license
```

`server_url` lấy từ config, path `/api/agent/report-license` được nối vào (dấu `/` cuối `server_url` nếu có sẽ bị cắt trước khi nối).

## 3. Headers

```
Content-Type: application/json
X-Api-Key: {api_key}
```

## 4. Request Body (payload thật — chỉ 3 field)

```json
{
  "machineGuid": "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx",
  "windowsStatus": "VALID",
  "officeStatus": "VALID"
}
```

**Không có** hostname, department, os_version, os_edition, timestamp, kmsStatus, windowsEdition, officeEdition, isError, errorMessage như tài liệu mock mô tả.

### Vocabulary trạng thái (đã xác nhận với BE team cho Windows)

| Enum nội bộ (`LicenseStatus`) | Giá trị gửi lên server |
|---|---|
| `Legitimate` | `"VALID"` |
| `Cracked` | `"CRACKED"` |
| `NotLicensed` | `"NOT_ACTIVATED"` |
| `UnableToDetermine` / khác | `"UNKNOWN"` |

- **`windowsStatus`**: map trực tiếp 1-1 từ enum trên — đã confirm với BE.
- **`officeStatus`**: Office không có enum riêng, agent chỉ có raw token từ `cscript ospp.vbs /dstatus` (ví dụ `LICENSE STATUS: ---LICENSED---`). Map theo heuristic (**chưa confirm với backend thật**, best-effort):
  - chứa `NON_GENUINE` → `CRACKED`
  - chứa `LICENSED` → `VALID`
  - chứa `GRACE` / `NOTIFICATIONS` / `HOLD` → `NOT_ACTIVATED`
  - còn lại → `UNKNOWN`

## 5. `machineGuid` lấy từ đâu

Đọc registry:
```
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Cryptography
  giá trị: MachineGuid  (REG_SZ)
```
Nếu không đọc được → gửi chuỗi rỗng `""`.

## 6. Config file (`agent_config.json`, đặt cạnh file .exe của agent)

Đọc **một lần** khi `ServerReporter` khởi tạo. Nếu file không tồn tại hoặc không parse được url+key hợp lệ → **tắt hoàn toàn việc report** (không gửi gì, không lỗi).

Code chấp nhận nhiều tên field khác nhau (do các bản config cũ/mới lẫn nhau):

| Mục đích | Field được đọc theo thứ tự ưu tiên |
|---|---|
| Server URL | `server_url` → nếu không có thì `serverUrl` |
| API key | `api_key` → nếu không có thì `X-Api-Key` → nếu không có thì `apiKey` |

Ví dụ config thật đang dùng ([deploy/real_test/agent_config.json](deploy/real_test/agent_config.json)):
```json
{
  "enabled": true,
  "server_url": "http://10.0.12.52",
  "api_key": "Xz6i3Qm_9dPCWuA0DbpLcPe4uMWnkwXJvEBHPo5HsYg",
  "department": "Default",
  "payload_format": "real"
}
```

⚠️ **Field không có tác dụng gì trong code hiện tại:** `enabled`, `department`, `payload_format`, `orgUnitId`, `orgUnitName` — các field này xuất hiện trong nhiều file config deploy nhưng `ServerReporter::LoadConfig()` **không đọc** chúng. Việc report có chạy hay không chỉ phụ thuộc vào: file tồn tại + rút được `server_url`/`api_key` hợp lệ.

## 7. Khi nào agent gửi

- `LicenseDetectionWorker` chạy detect theo timer, mặc định **5 phút/lần** (constructor nhận `std::chrono::seconds interval`, không đọc từ `agent_config.json`)
- Mỗi chu kỳ detect xong → gọi `reporter_->SendReport(result)` một lần (hàm tự no-op nếu chưa cấu hình)
- Không có cơ chế retry/backoff riêng — thất bại thì chờ chu kỳ sau

## 8. Xử lý response

- HTTP 2xx → coi là thành công, log info
- Khác 2xx → đọc tối đa 2048 byte body để log chẩn đoán, kèm status code
- Lỗi tầng network (send/receive fail) → log `GetLastError()`

## 9. Log để debug

Mỗi lần gửi, agent log đầy đủ request thật (qua `DetectionLogger`) để so sánh với `curl`/PowerShell khi server từ chối request:
- Full URL đã build lại
- Header (`Content-Type`, và **giá trị `X-Api-Key` ở dạng cleartext**)
- Body JSON

---

## Tóm tắt nhanh

```
POST {server_url}/api/agent/report-license
Content-Type: application/json
X-Api-Key: {api_key}

{"machineGuid":"...","windowsStatus":"VALID|CRACKED|NOT_ACTIVATED|UNKNOWN","officeStatus":"VALID|CRACKED|NOT_ACTIVATED|UNKNOWN"}
```
Chu kỳ: mỗi 5 phút, không retry, không gửi hostname/department/timestamp.
