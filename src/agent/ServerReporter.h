#pragma once

#include <string>
#include "../license-detection/LicenseResult.h"

class DetectionLogger;

// Reports license detection results (machine identity + Windows/Office
// license info) to a remote server over HTTP via WinHTTP.
//
// Fully self-contained: construction reads an optional config file next to
// the running executable, and SendReport() never throws - all config/WinHTTP
// failures are caught internally and logged, so it's always safe for the
// caller to invoke unconditionally once per detection cycle.
class ServerReporter {
public:
    explicit ServerReporter(DetectionLogger& logger);

    // Send a report for the given detection result. No-op if unconfigured
    // (no agent_config.json, or no usable server_url/api_key in it).
    void SendReport(const LicenseResult& result);

private:
    struct Config {
        bool enabled = false;
        std::string serverUrl;
        std::string apiKey;
    };

    Config LoadConfig() const;
    std::string BuildPayload(const LicenseResult& result) const;
    bool HttpPost(const std::string& jsonBody, std::string& outError) const;

    static std::string GetExecutableDirectory();
    static bool TryGetJsonString(const std::string& json, const std::string& key, std::string& outValue);
    static std::string GetMachineGuid();
    static std::string JsonEscape(const std::string& value);

    DetectionLogger& logger_;
    Config config_;
};
