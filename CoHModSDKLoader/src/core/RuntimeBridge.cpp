#include "RuntimeBridge.hpp"

#include <Windows.h>

#include <filesystem>

#include "../../../CoHModSDKRuntime/include/CoHModSDK.hpp"
#include "Loader.hpp"

namespace {
    constexpr char kRuntimeDllName[] = "CoHModSDKRuntime.dll";
    constexpr char kRuntimeLogPath[] = "mods/logs/sdk-runtime.log";
    constexpr char kGameModuleName[] = "WW2Mod.dll";

    using RuntimeInitializeFn = bool(*)(const CoHModSDKRuntimeInitV1* init);
    using RuntimeShutdownFn = void(*)();
    using RuntimeRegisterModFn = bool(*)(HMODULE modHandle, const CoHModSDKModuleV1* module, const CoHModSDKModContextV1** outContext);
    using RuntimeUnregisterModFn = void(*)(HMODULE modHandle);

    HMODULE runtimeModule = nullptr;
    RuntimeShutdownFn fnRuntimeShutdown = nullptr;
    RuntimeRegisterModFn fnRuntimeRegisterMod = nullptr;
    RuntimeUnregisterModFn fnRuntimeUnregisterMod = nullptr;
}

namespace Loader {
    void LoadRuntime() {
        if (runtimeModule != nullptr) {
            return;
        }

        const std::filesystem::path runtimePath = GetRelativePath(kRuntimeDllName);
        runtimeModule = LoadLibraryA(runtimePath.string().c_str());
        if (runtimeModule == nullptr) {
            FailFast("Failed to load CoHModSDKRuntime.dll");
        }

        const auto runtimeInitialize = reinterpret_cast<RuntimeInitializeFn>(GetProcAddress(runtimeModule, "CoHModSDKRuntime_Initialize"));
        fnRuntimeShutdown = reinterpret_cast<RuntimeShutdownFn>(GetProcAddress(runtimeModule, "CoHModSDKRuntime_Shutdown"));
        fnRuntimeRegisterMod = reinterpret_cast<RuntimeRegisterModFn>(GetProcAddress(runtimeModule, "CoHModSDKRuntime_RegisterMod"));
        fnRuntimeUnregisterMod = reinterpret_cast<RuntimeUnregisterModFn>(GetProcAddress(runtimeModule, "CoHModSDKRuntime_UnregisterMod"));
        if ((runtimeInitialize == nullptr) || (fnRuntimeShutdown == nullptr) || (fnRuntimeRegisterMod == nullptr) || (fnRuntimeUnregisterMod == nullptr)) {
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
        if (fnRuntimeShutdown != nullptr) {
            fnRuntimeShutdown();
        }
    }

    bool RegisterModWithRuntime(HMODULE modHandle, const CoHModSDKModuleV1* module, const CoHModSDKModContextV1** outContext) {
        return (fnRuntimeRegisterMod != nullptr) && fnRuntimeRegisterMod(modHandle, module, outContext);
    }

    void UnregisterModWithRuntime(HMODULE modHandle) {
        if (fnRuntimeUnregisterMod != nullptr) {
            fnRuntimeUnregisterMod(modHandle);
        }
    }
}
