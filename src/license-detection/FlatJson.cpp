#include "FlatJson.h"

#include <cctype>

namespace {

// Finds `"key":` in json and returns the index right after the colon, or
// std::string::npos if the key isn't present.
size_t FindValueStart(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) {
        return std::string::npos;
    }
    return pos + pattern.size();
}

} // namespace

bool FlatJson::GetString(const std::string& json, const std::string& key, std::string& outValue) {
    size_t pos = FindValueStart(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '"') {
        return false;
    }
    std::string value;
    size_t i = pos + 1;
    while (i < json.size() && json[i] != '"') {
        if (json[i] == '\\' && i + 1 < json.size()) {
            i++; // un-escape: keep the character after the backslash as-is
        }
        value.push_back(json[i]);
        i++;
    }
    outValue = value;
    return true;
}

bool FlatJson::GetBool(const std::string& json, const std::string& key, bool& outValue) {
    size_t pos = FindValueStart(json, key);
    if (pos == std::string::npos) {
        return false;
    }
    if (json.compare(pos, 4, "true") == 0) {
        outValue = true;
        return true;
    }
    if (json.compare(pos, 5, "false") == 0) {
        outValue = false;
        return true;
    }
    return false;
}

bool FlatJson::GetInt(const std::string& json, const std::string& key, int& outValue) {
    size_t pos = FindValueStart(json, key);
    if (pos == std::string::npos) {
        return false;
    }
    size_t i = pos;
    bool negative = false;
    if (i < json.size() && json[i] == '-') {
        negative = true;
        i++;
    }
    size_t digitsStart = i;
    long value = 0;
    while (i < json.size() && std::isdigit(static_cast<unsigned char>(json[i]))) {
        value = value * 10 + (json[i] - '0');
        i++;
    }
    if (i == digitsStart) {
        return false;
    }
    outValue = negative ? -static_cast<int>(value) : static_cast<int>(value);
    return true;
}
