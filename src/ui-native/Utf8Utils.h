#pragma once

#include <string>

// Small wide/narrow string conversion helpers shared by MainWindow and
// ResultsWindow wherever a Win32 dialog (MessageBoxW, GetSaveFileNameW)
// meets this codebase's UTF-8 std::strings (error messages, LocalDatabase/
// ExcelReport paths and text).
namespace Utf8Utils {

// LocalDatabase/ExcelReport return plain UTF-8 std::strings (including
// Vietnamese text) for display in a MessageBoxW - proper UTF-8 decoding,
// not the naive per-byte cast Localization::Widen does for ASCII-only UI
// strings.
std::wstring Utf8ToWide(const std::string& utf8);

// miniz's zip file functions (used by ExcelReport) go through the ANSI CRT
// (fopen(const char*)), which interprets narrow strings via the system
// codepage - same as every *A Win32 API this codebase already calls
// (GetModuleFileNameA, etc). So a path from a wide dialog result needs the
// ANSI codepage here, not UTF-8, or a path containing non-ASCII characters
// (a Vietnamese username/folder name, not unlikely for this app's userbase)
// would silently resolve to the wrong file.
std::string WideToAnsiPath(const std::wstring& wide);

} // namespace Utf8Utils
