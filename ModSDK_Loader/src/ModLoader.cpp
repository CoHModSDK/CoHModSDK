#include "ModLoader.hpp"

#include <Windows.h>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>

#include "LoaderRuntime.hpp"
#include "utils/Logger.hpp"

namespace {
    constexpr char kLoaderConfigName[] = "CoHModSDKLoader.ini";
    constexpr char kModsDirectoryName[] = "mods";
    constexpr char kLoaderLogPath[] = "mods/logs/sdk-loader.log";

    using OnSDKLoadFunc = void(*)();
    using OnGameStartFunc = void(*)();
    using OnGameShutdownFunc = void(*)();
    using GetModStringFunc = const char*(*)();

    Logger logger;
    std::vector<std::pair<std::string, HMODULE>> g_loadedMods;

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

    std::string GetExportedModString(HMODULE modHandle, const char* exportName) {
        const auto exportFunction = reinterpret_cast<GetModStringFunc>(GetProcAddress(modHandle, exportName));
        if (exportFunction == nullptr) {
            return {};
        }

        const char* value = exportFunction();
        if (value == nullptr) {
            return {};
        }

        return value;
    }

    void LogModMetadata(HMODULE modHandle, const std::string& name) {
        const std::string modName = GetExportedModString(modHandle, "GetModName");
        const std::string modVersion = GetExportedModString(modHandle, "GetModVersion");
        const std::string modAuthor = GetExportedModString(modHandle, "GetModAuthor");

        if (modName.empty() && modVersion.empty() && modAuthor.empty()) {
            return;
        }

        std::string message = "Mod metadata for " + name + ": ";
        message += "name=" + (modName.empty() ? std::string("<unknown>") : modName);
        message += ", version=" + (modVersion.empty() ? std::string("<unknown>") : modVersion);
        message += ", author=" + (modAuthor.empty() ? std::string("<unknown>") : modAuthor);
        GetLogger().LogInfo(message);
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
            if ((line.empty()) || (line[0] == '#')) {
                continue;
            }

            const std::string path = (modsDirectory / line).string();
            HMODULE modHandle = LoadLibraryA(path.c_str());
            if (!modHandle) {
                GetLogger().LogError("Failed to load mod: " + line);
                continue;
            }

            g_loadedMods.push_back(std::make_pair(line, modHandle));
            GetLogger().LogInfo("Loaded mod: " + line);
            LogModMetadata(modHandle, line);

            if (auto onLoad = reinterpret_cast<OnSDKLoadFunc>(GetProcAddress(modHandle, "OnSDKLoad"))) {
                onLoad();
				GetLogger().LogDebug("Called OnSDKLoad for " + line);
            }
        }
    }

    void NotifyModsGameStart() {
        for (auto& modEntry : g_loadedMods) {
            if (auto onStart = reinterpret_cast<OnGameStartFunc>(GetProcAddress(modEntry.second, "OnGameStart"))) {
                onStart();
                GetLogger().LogDebug("Called OnGameStart for " + modEntry.first);
            }
        }
    }

    void NotifyModsGameShutdown() {
        for (auto& modEntry : g_loadedMods) {
            if (auto onShutdown = reinterpret_cast<OnGameShutdownFunc>(GetProcAddress(modEntry.second, "OnGameShutdown"))) {
                onShutdown();
                GetLogger().LogDebug("Called OnGameShutdown for " + modEntry.first);
            }
        }
    }
}
