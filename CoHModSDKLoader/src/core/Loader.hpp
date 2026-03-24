#pragma once

#include <Windows.h>

#include <filesystem>
#include <string>

namespace Loader {
    void SetModuleHandle(HMODULE loaderModule);
    void EnsureInitialized();
    void Shutdown();
    void LoadOriginalDll();

    [[noreturn]] void FailFast(const std::string& message);
    std::filesystem::path GetDirectory();
    std::filesystem::path GetRelativePath(const char* fileName);
}
