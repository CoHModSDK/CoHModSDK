#include "RuntimeBridge.hpp"

#include <Windows.h>

#include <filesystem>

#include "../../ModSDK_Core/include/CoHModSDK.hpp"
#include "LoaderRuntime.hpp"

namespace {
    constexpr char kRuntimeDllName[] = "CoHModSDKRuntime.dll";
    constexpr char kRuntimeLogPath[] = "mods/logs/sdk-runtime.log";
    constexpr char kGameModuleName[] = "WW2Mod.dll";

    using RuntimeInitializeFunc = bool(*)(const CoHModSDKRuntimeInitV1* init);
    using RuntimeShutdownFunc = void(*)();
    using RuntimeRegisterModFunc = bool(*)(HMODULE modHandle, const CoHModSDKModuleV1* module, const CoHModSDKModContextV1** outContext);
    using RuntimeUnregisterModFunc = void(*)(HMODULE modHandle);

    HMODULE g_runtimeModule = nullptr;
    RuntimeShutdownFunc g_runtimeShutdown = nullptr;
    RuntimeRegisterModFunc g_runtimeRegisterMod = nullptr;
    RuntimeUnregisterModFunc g_runtimeUnregisterMod = nullptr;
}

namespace Loader {
    void LoadRuntime() {
        if (g_runtimeModule != nullptr) {
            return;
        }

        const std::filesystem::path runtimePath = GetRelativePath(kRuntimeDllName);
        g_runtimeModule = LoadLibraryA(runtimePath.string().c_str());
        if (g_runtimeModule == nullptr) {
            FailFast("Failed to load CoHModSDKRuntime.dll");
        }

        const auto runtimeInitialize = reinterpret_cast<RuntimeInitializeFunc>(GetProcAddress(g_runtimeModule, "CoHModSDKRuntime_Initialize"));
        g_runtimeShutdown = reinterpret_cast<RuntimeShutdownFunc>(GetProcAddress(g_runtimeModule, "CoHModSDKRuntime_Shutdown"));
        g_runtimeRegisterMod = reinterpret_cast<RuntimeRegisterModFunc>(GetProcAddress(g_runtimeModule, "CoHModSDKRuntime_RegisterMod"));
        g_runtimeUnregisterMod = reinterpret_cast<RuntimeUnregisterModFunc>(GetProcAddress(g_runtimeModule, "CoHModSDKRuntime_UnregisterMod"));
        if ((runtimeInitialize == nullptr) || (g_runtimeShutdown == nullptr) || (g_runtimeRegisterMod == nullptr) || (g_runtimeUnregisterMod == nullptr)) {
            FailFast("CoHModSDKRuntime.dll is missing required exports");
        }

        const std::filesystem::path loaderDirectory = GetDirectory();
        const std::filesystem::path modsDirectory = GetRelativePath("mods");
        const std::filesystem::path configDirectory = GetRelativePath("mods/config");
        const std::filesystem::path logPath = GetRelativePath(kRuntimeLogPath);
        const std::string loaderDirectoryString = loaderDirectory.string();
        const std::string modsDirectoryString = modsDirectory.string();
        const std::string configDirectoryString = configDirectory.string();
        const std::string logPathString = logPath.string();

        CoHModSDKRuntimeInitV1 init = {};
        init.abiVersion = COHMODSDK_ABI_VERSION;
        init.size = sizeof(CoHModSDKRuntimeInitV1);
        init.loaderDirectory = loaderDirectoryString.c_str();
        init.modsDirectory = modsDirectoryString.c_str();
        init.configDirectory = configDirectoryString.c_str();
        init.logPath = logPathString.c_str();
        init.gameModuleName = kGameModuleName;

        if (!runtimeInitialize(&init)) {
            FailFast("CoHModSDKRuntime.dll failed to initialize");
        }
    }

    void ShutdownRuntime() {
        if (g_runtimeShutdown != nullptr) {
            g_runtimeShutdown();
        }
    }

    bool RegisterModWithRuntime(HMODULE modHandle, const CoHModSDKModuleV1* module, const CoHModSDKModContextV1** outContext) {
        return (g_runtimeRegisterMod != nullptr) && g_runtimeRegisterMod(modHandle, module, outContext);
    }

    void UnregisterModWithRuntime(HMODULE modHandle) {
        if (g_runtimeUnregisterMod != nullptr) {
            g_runtimeUnregisterMod(modHandle);
        }
    }
}
