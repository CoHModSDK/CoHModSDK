#include "RuntimeState.hpp"

#include <Windows.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "../config/ConfigRegistry.hpp"
#include "../hooks/HookEngine.hpp"
#include "../utils/Logger.hpp"

struct CoHModSDKModContextV1 {
    HMODULE moduleHandle = nullptr;
};

namespace Runtime {
    namespace {
        constexpr char kRuntimeVersion[] = "0.1.0";

        struct RegisteredMod {
            std::string modId;
            std::string title;
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
            Logger logger;
            Config::Registry configRegistry;
        };

        State& GetState() {
            static State state;
            return state;
        }

        std::vector<int> PatternToBytes(const char* pattern) {
            std::vector<int> bytes;
            char* start = const_cast<char*>(pattern);
            char* end = const_cast<char*>(pattern) + std::strlen(pattern);

            for (char* current = start; current < end; ++current) {
                if (*current == '?') {
                    ++current;
                    if ((current < end) && (*current == '?')) {
                        ++current;
                    }

                    bytes.push_back(-1);
                    if (current >= end) {
                        break;
                    }
                } else if (*current != ' ') {
                    bytes.push_back(std::strtoul(current, &current, 16));
                }
            }

            return bytes;
        }

        void ShowRuntimeError(const std::string& message) {
            MessageBoxA(nullptr, message.c_str(), "CoHModSDK Runtime Error", MB_ICONERROR);
        }

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

    Logger& GetLogger() {
        return GetState().logger;
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

    std::uintptr_t FindPattern(const char* moduleName, const char* signature, bool reportError) {
        if (signature == nullptr) {
            return 0;
        }

        State& state = GetState();
        const char* effectiveModuleName = moduleName;
        if ((effectiveModuleName == nullptr) || (*effectiveModuleName == '\0')) {
            effectiveModuleName = state.gameModuleName.c_str();
        }

        HMODULE moduleHandle = GetModuleHandleA(effectiveModuleName);
        if (moduleHandle == nullptr) {
            if (reportError) {
                const std::string message = "Unable to get a handle for module '" + std::string(effectiveModuleName) + "'";
                state.logger.LogError(message);
                ShowRuntimeError(message);
                throw std::runtime_error(message);
            }

            return 0;
        }

        PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(moduleHandle);
        PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<std::uint8_t*>(moduleHandle) + dosHeader->e_lfanew);
        const std::size_t sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
        std::vector<int> patternBytes = PatternToBytes(signature);
        std::uint8_t* scanBytes = reinterpret_cast<std::uint8_t*>(moduleHandle);

        const std::size_t patternSize = patternBytes.size();
        if ((patternSize == 0u) || (sizeOfImage < patternSize)) {
            return 0;
        }

        for (std::size_t index = 0; index <= sizeOfImage - patternSize; ++index) {
            bool found = true;
            for (std::size_t patternIndex = 0; patternIndex < patternSize; ++patternIndex) {
                const int expectedByte = patternBytes[patternIndex];
                if ((expectedByte != -1) && (scanBytes[index + patternIndex] != expectedByte)) {
                    found = false;
                    break;
                }
            }

            if (found) {
                return reinterpret_cast<std::uintptr_t>(&scanBytes[index]);
            }
        }

        if (reportError) {
            const std::string message = "Unknown signature in module " + std::string(effectiveModuleName) + ": " + signature;
            state.logger.LogError(message);
            ShowRuntimeError(message);
            throw std::runtime_error(message);
        }

        return 0;
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

    bool RegisterConfigSchema(const CoHModSDKConfigSchemaV1* schema) {
        return GetState().configRegistry.RegisterSchema(schema);
    }

    bool GetConfigValue(const char* modId, const char* optionId, CoHModSDKConfigValueV1* outValue) {
        return GetState().configRegistry.GetValue(modId, optionId, outValue);
    }

    bool SetConfigValue(const char* modId, const char* optionId, const CoHModSDKConfigValueV1* value) {
        return GetState().configRegistry.SetValue(modId, optionId, value);
    }

    bool EnumerateConfigMods(CoHModSDKConfigModVisitor visitor, void* userData) {
        return GetState().configRegistry.EnumerateMods(visitor, userData);
    }

    bool EnumerateConfigOptions(const char* modId, CoHModSDKConfigOptionVisitor visitor, void* userData) {
        return GetState().configRegistry.EnumerateOptions(modId, visitor, userData);
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
