#pragma once

#include <string>

namespace DyingLightHeadTracking {

// Wide path to a file beside this DLL. Empty when the directory is unknown.
// Wide throughout: the real path is UTF-16, and GetModuleFileNameA would replace
// anything outside the active ANSI codepage with '?' before we ever saw it.
std::wstring GetModulePathW(const char* filename);

// Narrow form of the same path, for the core APIs that take std::string. Empty
// when the directory is unknown, or when the path does not survive the ANSI
// codepage - callers must not fall back to a bare filename, which
// GetPrivateProfileString would resolve against the Windows directory.
std::string GetModulePath(const char* filename);

}  // namespace DyingLightHeadTracking
