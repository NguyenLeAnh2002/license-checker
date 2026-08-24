#include "Localization.h"

static Language g_currentLanguage = Language::Vietnamese;

struct StringMapping {
    const wchar_t* vietnamese;
    const wchar_t* english;
};

static StringMapping g_strings[] = {
    { L"Kiểm Tra Bản Quyền", L"License Checker" },                                    // WindowTitle
    { L"Bản Quyền Windows", L"Windows License" },                                      // TabWindows
    { L"Bản Quyền Office", L"Office License" },                                        // TabOffice
    { L"Cài Đặt", L"Settings" },                                                       // TabSettings
    { L"Kiểm Tra Ngay", L"Check Now" },                                                // CheckNow
    { L"Sẵn Sàng", L"Ready" },                                                         // Ready
    { L"Đang Kiểm Tra...", L"Checking..." },                                           // Checking
    { L"Không kết nối được dịch vụ", L"Service not connected" },                       // ServiceNotConnected
    { L"Không lấy được dữ liệu", L"Failed to get data" },                              // FailedToGetData
    { L"Không thể gửi yêu cầu kiểm tra", L"Failed to queue check" },                   // FailedToQueueCheck
    { L"Lỗi trong khi kiểm tra", L"Error during check" },                              // ErrorDuringCheck
    { L"Lỗi không xác định khi kiểm tra", L"Unknown error during check" },             // UnknownErrorDuringCheck
    { L"Cập nhật lần cuối: vừa xong", L"Last updated: now" },                          // LastUpdatedNow
    { L"Lỗi phân tích dữ liệu", L"Parse error" },                                      // ParseError
    { L"Tên:", L"Name:" },                                                             // Name
    { L"Phiên bản:", L"Edition:" },                                                    // Edition
    { L"Mô tả:", L"Description:" },                                                    // Description
    { L"Trạng thái bản quyền:", L"License Status:" },                                  // LicenseStatus
    { L"Khóa sản phẩm:", L"Product Key:" },                                            // ProductKey
    { L"Trạng thái KMS:", L"KMS Status:" },                                            // KmsStatus
    { L"Máy chủ KMS:", L"KMS Server:" },                                               // KmsServer
    { L"Lần phát hiện cuối:", L"Last Detected:" },                                     // LastDetected
    { L"Trạng thái:", L"Status:" },                                                    // Status
    { L"Tên bản quyền:", L"License Name:" },                                           // LicenseName
    { L"Thời gian ân hạn:", L"Grace Period:" },                                        // GracePeriod
    { L"Chu kỳ kích hoạt:", L"Activation Interval:" },                                 // ActivationInterval
    { L"Tên máy:", L"Hostname:" },                                                     // Hostname
    { L"Mã định danh máy (GUID):", L"Machine GUID:" },                                 // MachineGuid
    { L"Phòng ban:", L"Department:" },                                                 // Department
    { L"Trạng thái dịch vụ:", L"Service Status:" },                                    // ServiceStatus
    { L"Trực tuyến", L"Online" },                                                      // Online
    { L"Ngoại tuyến", L"Offline" },                                                    // Offline
    { L"Không xác định", L"Unknown" },                                                 // Unknown
    { L"Chưa từng", L"Never" },                                                        // Never
    { L"Không phát hiện", L"Not Detected" },                                           // NotDetected
    { L"Không dùng KMS", L"Not Using KMS" },                                           // KmsNotUsing
    { L"Đã phát hiện KMS", L"KMS Detected" },                                          // KmsDetected
    { L"Không tìm thấy KMS", L"KMS Not Found" },                                       // KmsNotFound
    { L"Lỗi", L"Error" },                                                              // KmsError
    { L"Ngôn ngữ:", L"Language:" }                                                     // LanguageLabel
};

const wchar_t* Localization::T(Str id) {
    int index = static_cast<int>(id);
    if (index < 0 || index >= static_cast<int>(sizeof(g_strings) / sizeof(g_strings[0]))) {
        return L"";
    }
    if (g_currentLanguage == Language::Vietnamese) {
        return g_strings[index].vietnamese;
    } else {
        return g_strings[index].english;
    }
}

std::wstring Localization::TranslateStatusValue(const std::string& raw) {
    if (raw == "Legitimate") {
        return g_currentLanguage == Language::Vietnamese ? L"Hợp lệ (chính hãng)" : L"Legitimate";
    } else if (raw == "Cracked") {
        return g_currentLanguage == Language::Vietnamese ? L"Bị bẻ khóa (lậu)" : L"Cracked";
    } else if (raw == "NotLicensed") {
        return g_currentLanguage == Language::Vietnamese ? L"Chưa có bản quyền" : L"Not Licensed";
    } else if (raw == "UnableToDetermine") {
        return g_currentLanguage == Language::Vietnamese ? L"Không xác định được" : L"Unable to Determine";
    } else if (raw == "Licensed") {
        return g_currentLanguage == Language::Vietnamese ? L"Đã có bản quyền" : L"Licensed";
    } else if (raw == "GRACE") {
        return g_currentLanguage == Language::Vietnamese ? L"Đang trong thời gian ân hạn" : L"Grace Period";
    }
    return Widen(raw);
}

std::wstring Localization::Widen(const std::string& ascii) {
    std::wstring result;
    for (unsigned char c : ascii) {
        result.push_back(static_cast<wchar_t>(c));
    }
    return result;
}

void Localization::SetLanguage(Language lang) {
    g_currentLanguage = lang;
}

Language Localization::GetLanguage() {
    return g_currentLanguage;
}
