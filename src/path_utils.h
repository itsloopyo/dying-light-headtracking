#pragma once

#include <string>

namespace DyingLightHeadTracking {

// The folder this DLL was loaded from, with its trailing separator. Empty when the directory is
// unknown. Wide throughout: the real path is UTF-16, and GetModuleFileNameA would replace
// anything outside the active ANSI codepage with '?' before we ever saw it.
std::wstring GetModuleDirectoryW();

// Wide path to a file beside this DLL. Empty when the directory is unknown.
std::wstring GetModulePathW(const char* filename);

// The ANSI path v0.1.0 opened @p path by, for the legacy import: the path itself where the
// active ANSI codepage holds every character of its folder, else with the folder's 8.3 short
// name. Empty where neither gives a path or the result does not fit MAX_PATH; v0.1.0 did not
// start there.
std::string LegacyAnsiPath(const std::wstring& path);

}  // namespace DyingLightHeadTracking
