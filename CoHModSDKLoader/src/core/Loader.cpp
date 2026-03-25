#include "Loader.hpp"

#include <Windows.h>

#include <cstdlib>
#include <filesystem>

#include "exports/ProxyExports.hpp"
#include "mods/ModLoader.hpp"
#include "RuntimeBridge.hpp"

namespace {
    HMODULE loaderModule = nullptr;
    INIT_ONCE initializeOnce = INIT_ONCE_STATIC_INIT;
    HMODULE originalWW2ModDll = nullptr;

    void ResolveOriginalExports() {
        const auto getDllInterface = reinterpret_cast<Loader::GetDllInterfaceFn>(GetProcAddress(originalWW2ModDll, "GetDllInterface"));
        const auto getDllVersion = reinterpret_cast<Loader::GetDllVersionFn>(GetProcAddress(originalWW2ModDll, "GetDllVersion"));

        if ((getDllInterface == nullptr) || (getDllVersion == nullptr)) {
            Loader::FailFast("WW2Mod.dll is missing one or more required exports");
        }

        Loader::SetGetDllInterfaceExportTarget(getDllInterface);
        Loader::SetGetDllVersionExportTarget(getDllVersion);
    }

    BOOL CALLBACK InitializeOnceProc(PINIT_ONCE, PVOID, PVOID*) {
        Loader::LoadRuntime();
        Loader::LoadConfiguredMods();
        Loader::NotifyModsLoaded();
        return TRUE;
    }
}

namespace Loader {
    void SetModuleHandle(HMODULE moduleHandle) {
        loaderModule = moduleHandle;
    }

    void EnsureInitialized() {
        PVOID context = nullptr;
        if (!InitOnceExecuteOnce(&initializeOnce, &InitializeOnceProc, nullptr, &context)) {
            FailFast("Failed to initialize CoHModSDK loader");
        }
    }

    void Shutdown() {
        NotifyModsShutdown();
        ShutdownRuntime();
    }

    void LoadOriginalDll() {
        const std::filesystem::path originalDllPath = GetRelativePath("WW2Mod.dll");
        originalWW2ModDll = LoadLibraryA(originalDllPath.string().c_str());
        if (!originalWW2ModDll) {
            FailFast("Failed to load WW2Mod.dll");
        }

        ResolveOriginalExports();
    }

    [[noreturn]] void FailFast(const std::string& message) {
        MessageBoxA(nullptr, message.c_str(), "CoHModSDK Loader Error", MB_ICONERROR);
        ExitProcess(EXIT_FAILURE);
    }

    std::filesystem::path GetDirectory() {
        char modulePath[MAX_PATH] = {};
        if ((loaderModule == nullptr) || (GetModuleFileNameA(loaderModule, modulePath, MAX_PATH) == 0)) {
            FailFast("Failed to resolve the loader path");
        }

        return std::filesystem::path(modulePath).parent_path();
    }

    std::filesystem::path GetRelativePath(const char* fileName) {
        return GetDirectory() / fileName;
    }
}
