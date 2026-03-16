#include "LoaderRuntime.hpp"

#include "ModLoader.hpp"
#include "OriginalDll.hpp"

namespace {
    HMODULE g_loaderModule = nullptr;
}

namespace Loader {
    void SetModuleHandle(HMODULE loaderModule) {
        g_loaderModule = loaderModule;
    }

    void Initialize() {
        LoadOriginalDll();
        LoadConfiguredMods();
        NotifyModsGameStart();
    }

    void Shutdown() {
        NotifyModsGameShutdown();
    }

    [[noreturn]] void FailFast(const std::string& message) {
        MessageBoxA(nullptr, message.c_str(), "Error", MB_ICONERROR);
        ExitProcess(EXIT_FAILURE);
    }

    std::filesystem::path GetDirectory() {
        char modulePath[MAX_PATH] = {};
        if ((g_loaderModule == nullptr) || (GetModuleFileNameA(g_loaderModule, modulePath, MAX_PATH) == 0)) {
            FailFast("Failed to resolve the loader path");
        }

        return std::filesystem::path(modulePath).parent_path();
    }

    std::filesystem::path GetRelativePath(const char* fileName) {
        return GetDirectory() / fileName;
    }
}
