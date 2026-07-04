#include "Platform/PlatformPaths.h"
#include "WitchEngine/Core/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace witch::platform {

std::filesystem::path GetExecutableDir() {
    std::wstring buf(MAX_PATH, L'\0');
    DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    while (len == buf.size()) {
        buf.resize(buf.size() * 2);
        len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    }
    if (len == 0) {
        log::Error("GetExecutableDir: GetModuleFileNameW failed");
        return std::filesystem::current_path();
    }
    buf.resize(len);
    return std::filesystem::path(buf).parent_path();
}

} // namespace witch::platform
