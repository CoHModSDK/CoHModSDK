#pragma once

#include <filesystem>

#include "../../include/CoHModSDK.hpp"

class Logger;

namespace Runtime::Config {
    struct ParsedValue;
    struct StoredChoice;
    struct StoredOption;
    struct StoredModConfig;

    class Registry {
    public:
        void Initialize(const std::filesystem::path& configDirectory, Logger* logger);
        void Shutdown();

        bool RegisterSchema(const CoHModSDKConfigSchemaV1* schema);
        bool GetValue(const char* modId, const char* optionId, CoHModSDKConfigValueV1* outValue);
        bool SetValue(const char* modId, const char* optionId, const CoHModSDKConfigValueV1* value);
        bool EnumerateOptions(const char* modId, CoHModSDKConfigOptionVisitor visitor, void* userData);

    private:
        bool ValidateValue(const StoredOption& option, const CoHModSDKConfigValueV1& value) const;
        bool LoadPersistedValues(const std::string& modId, StoredModConfig& modConfig);
        bool SaveModConfig(const StoredModConfig& modConfig);
        std::filesystem::path GetConfigPath(const std::string& modId) const;
        void LogWarning(const std::string& message) const;
        void LogInfo(const std::string& message) const;

    private:
        std::filesystem::path configDirectory;
        Logger* logger = nullptr;
    };
}
