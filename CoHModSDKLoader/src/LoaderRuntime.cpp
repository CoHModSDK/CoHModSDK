#include "LoaderRuntime.hpp"

#include <Windows.h>

#include <filesystem>

#include "ModLoader.hpp"
#include "OriginalDll.hpp"
#include "RuntimeBridge.hpp"

namespace {
    HMODULE g_loaderModule = nullptr;
    INIT_ONCE g_initializeOnce = INIT_ONCE_STATIC_INIT;

    BOOL CALLBACK InitializeOnceProc(PINIT_ONCE, PVOID, PVOID*) {
        Loader::LoadRuntime();
        Loader::LoadConfiguredMods();
        Loader::NotifyModsLoaded();
        return TRUE;
    }
}

namespace Loader {
    void SetModuleHandle(HMODULE loaderModule) {
        g_loaderModule = loaderModule;
    }

    void EnsureInitialized() {
        PVOID context = nullptr;
        if (!InitOnceExecuteOnce(&g_initializeOnce, &InitializeOnceProc, nullptr, &context)) {
            FailFast("Failed to initialize CoHModSDK loader");
        }
    }

    void Shutdown() {
        NotifyModsShutdown();
        ShutdownRuntime();
    }

    [[noreturn]] void FailFast(const std::string& message) {
        MessageBoxA(nullptr, message.c_str(), "CoHModSDK Loader Error", MB_ICONERROR);
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
