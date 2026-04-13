#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "../../include/CoHModSDK.hpp"
#include "../config/ConfigRegistry.hpp"
#include "../hooks/HookEngine.hpp"
#include "../utils/Logger.hpp"

struct CoHModSDKModContextV1 {
    HMODULE moduleHandle = nullptr;
};

namespace Runtime {
    struct RegisteredMod {
        std::string modId;
        std::string title;
        std::string version;
        std::string author;
        CoHModSDKModContextV1 context = {};
    };

    struct State {
        std::mutex mutex;
        bool initialized = false;
        std::string loaderDirectory;
        std::string modsDirectory;
        std::string configDirectory;
        std::string logPath;
        std::string gameModuleName;
        std::unordered_map<HMODULE, std::unique_ptr<RegisteredMod>> registeredMods;
        CoHModSDKRuntimeInfoV1 runtimeInfo = {};
        Logger logger{"Runtime"};
        Config::Registry configRegistry;
        HookEngine hookEngine;
    };

    State& GetState();

    bool Initialize(const CoHModSDKRuntimeInitV1* init);
    void Shutdown();

    const CoHModSDKRuntimeInfoV1* GetRuntimeInfo();
    void LogForMod(const CoHModSDKModContextV1* modContext, CoHModSDKLogLevel level, const char* message);

    std::optional<std::uintptr_t> FindPattern(const char* moduleName, const char* signature);
    void PatchMemory(void* destination, const void* source, std::size_t size);
    void ShowModError(const CoHModSDKModContextV1* modContext, const char* message);

    bool GetRegisteredModInfo(const char* modId, CoHModSDKConfigModInfoV1* outInfo);
    bool RegisterMod(HMODULE modHandle, const CoHModSDKModuleV1* module, const CoHModSDKModContextV1** outContext);
    void UnregisterMod(HMODULE modHandle);
}
