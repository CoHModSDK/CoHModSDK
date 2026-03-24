#include "ModLoader.hpp"

#include <Windows.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "../../ModSDK_Core/include/CoHModSDK.hpp"
#include "LoaderRuntime.hpp"
#include "RuntimeBridge.hpp"
#include "utils/Logger.hpp"

namespace {
    constexpr char kLoaderConfigName[] = "CoHModSDKLoader.ini";
    constexpr char kModsDirectoryName[] = "mods";
    constexpr char kLoaderLogPath[] = "mods/logs/sdk-loader.log";

    using GetModModuleFunc = bool(*)(std::uint32_t abiVersion, const CoHModSDKModuleV1** outModule);
    using SetModContextFunc = void(*)(const CoHModSDKModContextV1* modContext);

    struct LoadedMod {
        std::string fileName;
        HMODULE handle = nullptr;
        const CoHModSDKModuleV1* module = nullptr;
    };

    Logger logger;
    std::vector<LoadedMod> g_loadedMods;

    Logger& GetLogger() {
        if (!logger.IsOpen()) {
            logger.Open(Loader::GetRelativePath(kLoaderLogPath));
        }

        return logger;
    }

    std::string Trim(std::string value) {
        const std::size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return {};
        }

        const std::size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    void LogModMetadata(const LoadedMod& loadedMod) {
        if (loadedMod.module == nullptr) {
            return;
        }

        std::string message = "Loaded mod metadata for " + loadedMod.fileName + ": ";
        message += "modid=" + std::string(loadedMod.module->modId == nullptr ? "<unknown>" : loadedMod.module->modId);
        message += ", name=" + std::string(loadedMod.module->name == nullptr ? "<unknown>" : loadedMod.module->name);
        message += ", version=" + std::string(loadedMod.module->version == nullptr ? "<unknown>" : loadedMod.module->version);
        message += ", author=" + std::string(loadedMod.module->author == nullptr ? "<unknown>" : loadedMod.module->author);
        GetLogger().LogInfo(message);
    }

    void UnloadMod(const LoadedMod& loadedMod, bool callShutdown) {
        if (callShutdown && (loadedMod.module != nullptr) && (loadedMod.module->OnShutdown != nullptr)) {
            loadedMod.module->OnShutdown();
        }

        if (loadedMod.handle != nullptr) {
            Loader::UnregisterModWithRuntime(loadedMod.handle);
            FreeLibrary(loadedMod.handle);
        }
    }

    bool TryGetValidatedModuleDescriptor(const std::string& fileName, GetModModuleFunc getModule, const CoHModSDKModuleV1*& outModule) {
        outModule = nullptr;

        if (!getModule(COHMODSDK_ABI_VERSION, &outModule)) {
            GetLogger().LogError(
                "Mod rejected the loader's ABI " + std::to_string(COHMODSDK_ABI_VERSION) +
                " module request and is likely built against a different CoHModSDK version: " + fileName
            );
            return false;
        }

        if (outModule == nullptr) {
            GetLogger().LogError("Mod returned a null module descriptor: " + fileName);
            return false;
        }

        if (outModule->abiVersion != COHMODSDK_ABI_VERSION) {
            GetLogger().LogError(
                "Mod reported ABI " + std::to_string(outModule->abiVersion) +
                ", but the loader requires ABI " + std::to_string(COHMODSDK_ABI_VERSION) +
                ". The mod is outdated or built against a different CoHModSDK version: " + fileName
            );
            return false;
        }

        if (outModule->size < sizeof(CoHModSDKModuleV1)) {
            GetLogger().LogError(
                "Mod returned a module descriptor of size " + std::to_string(outModule->size) +
                ", but the loader requires at least " + std::to_string(sizeof(CoHModSDKModuleV1)) +
                ". The mod is likely built against an older CoHModSDK module layout: " + fileName
            );
            return false;
        }

        return true;
    }
}

namespace Loader {
    void LoadConfiguredMods() {
        const std::filesystem::path modsDirectory = GetRelativePath(kModsDirectoryName);
        CreateDirectoryA(modsDirectory.string().c_str(), nullptr);

        std::ifstream config(GetRelativePath(kLoaderConfigName));
        if (!config.is_open()) {
            GetLogger().LogError(std::string(kLoaderConfigName) + " not found");
            return;
        }

        GetLogger().LogInfo(std::string(kLoaderConfigName) + " found, loading listed mods");

        std::string line;
        while (std::getline(config, line)) {
            line = Trim(std::move(line));
            if (line.empty() || (line[0] == '#')) {
                continue;
            }

            const std::string path = (modsDirectory / line).string();
            HMODULE modHandle = LoadLibraryA(path.c_str());
            if (modHandle == nullptr) {
                GetLogger().LogError("Failed to load mod: " + line);
                continue;
            }

            const auto getModule = reinterpret_cast<GetModModuleFunc>(GetProcAddress(modHandle, "CoHMod_GetModule"));
            if (getModule == nullptr) {
                GetLogger().LogError("Mod is missing CoHMod_GetModule export: " + line);
                FreeLibrary(modHandle);
                continue;
            }

            const auto setContext = reinterpret_cast<SetModContextFunc>(GetProcAddress(modHandle, "CoHMod_SetContext"));
            if (setContext == nullptr) {
                GetLogger().LogError("Mod is missing CoHMod_SetContext export: " + line);
                FreeLibrary(modHandle);
                continue;
            }

            const CoHModSDKModuleV1* module = nullptr;
            if (!TryGetValidatedModuleDescriptor(line, getModule, module)) {
                FreeLibrary(modHandle);
                continue;
            }

            LoadedMod loadedMod = {};
            loadedMod.fileName = line;
            loadedMod.handle = modHandle;
            loadedMod.module = module;

            if ((loadedMod.module->modId == nullptr) || (loadedMod.module->name == nullptr) || (loadedMod.module->version == nullptr) || (loadedMod.module->author == nullptr)) {
                GetLogger().LogError("Mod metadata is incomplete: " + line);
                FreeLibrary(modHandle);
                continue;
            }

            const CoHModSDKModContextV1* modContext = nullptr;
            if (!RegisterModWithRuntime(loadedMod.handle, loadedMod.module, &modContext) || (modContext == nullptr)) {
                GetLogger().LogError("Failed to register mod with runtime: " + line);
                FreeLibrary(modHandle);
                continue;
            }

            setContext(modContext);
            if ((loadedMod.module->OnInitialize != nullptr) && !loadedMod.module->OnInitialize()) {
                GetLogger().LogError("Mod OnInitialize failed: " + line);
                UnloadMod(loadedMod, true);
                continue;
            }

            g_loadedMods.push_back(loadedMod);
            GetLogger().LogInfo("Loaded mod: " + line);
            LogModMetadata(loadedMod);
        }
    }

    void NotifyModsLoaded() {
        for (std::size_t index = 0; index < g_loadedMods.size();) {
            const LoadedMod loadedMod = g_loadedMods[index];
            if ((loadedMod.module->OnModsLoaded != nullptr) && !loadedMod.module->OnModsLoaded()) {
                GetLogger().LogError("Mod OnModsLoaded failed: " + loadedMod.fileName);
                UnloadMod(loadedMod, true);
                g_loadedMods.erase(g_loadedMods.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }

            if (loadedMod.module->OnModsLoaded != nullptr) {
                GetLogger().LogDebug("Called OnModsLoaded for " + loadedMod.fileName);
            }

            ++index;
        }
    }

    void NotifyModsShutdown() {
        for (const LoadedMod& loadedMod : g_loadedMods) {
            if (loadedMod.module->OnShutdown != nullptr) {
                loadedMod.module->OnShutdown();
                GetLogger().LogDebug("Called OnShutdown for " + loadedMod.fileName);
            }

            UnregisterModWithRuntime(loadedMod.handle);
        }
    }
}
