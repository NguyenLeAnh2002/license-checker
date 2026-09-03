#include "Utf8Utils.h"

#include <windows.h>

std::wstring Utf8Utils::Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return L"";
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    if (size <= 0) {
        return L"";
    }
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], size);
    return result;
}

std::string Utf8Utils::WideToAnsiPath(const std::wstring& wide) {
    if (wide.empty()) {
        return "";
    }
    int size = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return "";
    }
    std::string result(size, 0);
    WideCharToMultiByte(CP_ACP, 0, wide.c_str(), (int)wide.size(), &result[0], size, nullptr, nullptr);
    return result;
}
