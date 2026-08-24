#pragma once

#include <string>
#include <cwchar>

enum class Language {
    Vietnamese,
    English
};

enum class Str {
    WindowTitle,
    TabWindows,
    TabOffice,
    TabSettings,
    CheckNow,
    Ready,
    Checking,
    ServiceNotConnected,
    FailedToGetData,
    FailedToQueueCheck,
    ErrorDuringCheck,
    UnknownErrorDuringCheck,
    LastUpdatedNow,
    ParseError,
    Name,
    Edition,
    Description,
    LicenseStatus,
    ProductKey,
    KmsStatus,
    KmsServer,
    LastDetected,
    Status,
    LicenseName,
    GracePeriod,
    ActivationInterval,
    Hostname,
    MachineGuid,
    Department,
    ServiceStatus,
    Online,
    Offline,
    Unknown,
    Never,
    NotDetected,
    KmsNotUsing,
    KmsDetected,
    KmsNotFound,
    KmsError,
    LanguageLabel
};

namespace Localization {
    const wchar_t* T(Str id);
    std::wstring TranslateStatusValue(const std::string& raw);
    std::wstring Widen(const std::string& ascii);
    void SetLanguage(Language lang);
    Language GetLanguage();
}
