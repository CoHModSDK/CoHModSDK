#include "RuntimeState.hpp"

#include <Windows.h>

#include <cstring>
#include <filesystem>

#include "../memory/PatternScanner.hpp"

namespace Runtime {
    namespace {
        constexpr char kRuntimeVersion[] = "0.4.0";

        HMODULE ResolveHandleFromContext(const CoHModSDKModContextV1* modContext) {
            if (modContext == nullptr) {
                return nullptr;
            }

            return modContext->moduleHandle;
        }

        std::string ResolveTitleFromHandle(State& state, HMODULE modHandle) {
            if (modHandle != nullptr) {
                const auto titleIterator = state.registeredMods.find(modHandle);
                if ((titleIterator != state.registeredMods.end()) && (titleIterator->second != nullptr)) {
                    return titleIterator->second->title;
                }
            }

            char modulePath[MAX_PATH] = {};
            if ((modHandle != nullptr) && (GetModuleFileNameA(modHandle, modulePath, MAX_PATH) != 0)) {
                return std::filesystem::path(modulePath).stem().string();
            }

            return "CoHModSDK Mod";
        }
    }

    State& GetState() {
        static State state;
        return state;
    }

    bool Initialize(const CoHModSDKRuntimeInitV1* init) {
        if ((init == nullptr) || (init->abiVersion != COHMODSDK_ABI_VERSION) || (init->size < sizeof(CoHModSDKRuntimeInitV1))) {
            return false;
        }

        if ((init->loaderDirectory == nullptr) || (init->modsDirectory == nullptr) || (init->configDirectory == nullptr) || (init->logPath == nullptr) || (init->gameModuleName == nullptr)) {
            return false;
        }

        State& state = GetState();
        std::scoped_lock lock(state.mutex);
        if (state.initialized) {
            return true;
        }

        state.loaderDirectory = init->loaderDirectory;
        state.modsDirectory = init->modsDirectory;
        state.configDirectory = init->configDirectory;
        state.logPath = init->logPath;
        state.gameModuleName = init->gameModuleName;

        state.logger.Open(state.logPath);
        state.configRegistry.Initialize(std::filesystem::path(state.configDirectory), &state.logger);

        state.runtimeInfo.abiVersion = COHMODSDK_ABI_VERSION;
        state.runtimeInfo.size = sizeof(CoHModSDKRuntimeInfoV1);
        state.runtimeInfo.runtimeVersion = kRuntimeVersion;
        state.runtimeInfo.loaderDirectory = state.loaderDirectory.c_str();
        state.runtimeInfo.modsDirectory = state.modsDirectory.c_str();
        state.runtimeInfo.configDirectory = state.configDirectory.c_str();
        state.runtimeInfo.logPath = state.logPath.c_str();
        state.runtimeInfo.gameModuleName = state.gameModuleName.c_str();

        state.initialized = true;
        state.logger.LogInfo("CoHModSDK runtime initialized");
        return true;
    }

    void Shutdown() {
        State& state = GetState();
        std::scoped_lock lock(state.mutex);
        if (!state.initialized) {
            return;
        }

        state.logger.LogInfo("CoHModSDK runtime shutting down");
        state.configRegistry.Shutdown();
        state.registeredMods.clear();
        state.initialized = false;
    }

    const CoHModSDKRuntimeInfoV1* GetRuntimeInfo() {
        State& state = GetState();
        std::scoped_lock lock(state.mutex);
        if (!state.initialized) {
            return nullptr;
        }

        return &state.runtimeInfo;
    }

    void LogForMod(const CoHModSDKModContextV1* modContext, CoHModSDKLogLevel level, const char* message) {
        if ((message == nullptr) || (*message == '\0')) {
            return;
        }

        State& state = GetState();
        std::string source;
        {
            std::scoped_lock lock(state.mutex);
            if (state.initialized) {
                source = ResolveTitleFromHandle(state, ResolveHandleFromContext(modContext));
            }
        }

        const char* sourceText = source.empty() ? nullptr : source.c_str();
        switch (level) {
        case CoHModSDKLogLevel_Debug:
            state.logger.LogDebug(message, sourceText);
            break;
        case CoHModSDKLogLevel_Info:
            state.logger.LogInfo(message, sourceText);
            break;
        case CoHModSDKLogLevel_Warning:
            state.logger.LogWarning(message, sourceText);
            break;
        case CoHModSDKLogLevel_Error:
            state.logger.LogError(message, sourceText);
            break;
        default:
            state.logger.LogInfo(message, sourceText);
            break;
        }
    }

    std::optional<std::uintptr_t> FindPattern(const char* moduleName, const char* signature) {
        return PatternScanner::Find(moduleName, signature, GetState().logger);
    }

    void PatchMemory(void* destination, const void* source, std::size_t size) {
        DWORD oldProtect = 0;
        VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE, &oldProtect);
        std::memcpy(destination, source, size);
        FlushInstructionCache(GetCurrentProcess(), destination, size);
        DWORD restoredProtect = 0;
        VirtualProtect(destination, size, oldProtect, &restoredProtect);
    }

    void ShowModError(const CoHModSDKModContextV1* modContext, const char* message) {
        if ((message == nullptr) || (*message == '\0')) {
            return;
        }

        const HMODULE modHandle = ResolveHandleFromContext(modContext);

        State& state = GetState();
        std::string title;
        {
            std::scoped_lock lock(state.mutex);
            title = ResolveTitleFromHandle(state, modHandle);
            if (state.initialized) {
                state.logger.LogError(message, title.c_str());
            }
        }

        MessageBoxA(nullptr, message, title.c_str(), MB_OK | MB_ICONERROR);
    }

    bool RegisterMod(HMODULE modHandle, const CoHModSDKModuleV1* module, const CoHModSDKModContextV1** outContext) {
        if ((modHandle == nullptr) || (module == nullptr) || (module->modId == nullptr) || (module->name == nullptr) || (outContext == nullptr)) {
            return false;
        }

        State& state = GetState();
        std::scoped_lock lock(state.mutex);
        if (!state.initialized) {
            return false;
        }

        if (state.registeredMods.contains(modHandle)) {
            return false;
        }

        auto registeredMod = std::make_unique<RegisteredMod>();
        registeredMod->modId = module->modId;
        registeredMod->title = module->name;
        registeredMod->context.moduleHandle = modHandle;

        *outContext = &registeredMod->context;
        state.registeredMods[modHandle] = std::move(registeredMod);
        return true;
    }

    void UnregisterMod(HMODULE modHandle) {
        if (modHandle == nullptr) {
            return;
        }

        State& state = GetState();
        std::scoped_lock lock(state.mutex);
        state.registeredMods.erase(modHandle);
    }
}
