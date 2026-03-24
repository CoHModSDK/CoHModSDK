#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../include/CoHModSDK.hpp"

class Logger;

namespace Runtime::Config {
    struct StoredChoice {
        std::int32_t value = 0;
        std::string valueId;
        std::string label;
    };

    struct StoredOption {
        std::string optionId;
        std::string category;
        std::string label;
        std::string description;
        CoHModSDKConfigType type = CoHModSDKConfigType_Bool;
        CoHModSDKConfigValueV1 defaultValue = {};
        CoHModSDKConfigValueV1 currentValue = {};
        float minValue = 0.0f;
        float maxValue = 0.0f;
        float step = 0.0f;
        std::uint32_t flags = CoHModSDKConfigFlags_None;
        std::vector<StoredChoice> choices;
        std::vector<CoHModSDKConfigChoiceV1> choiceViews;
        CoHModSDKConfigChangedCallback onChanged = nullptr;
        void* userData = nullptr;
        CoHModSDKConfigOptionV1 view = {};
    };

    struct StoredModConfig {
        std::string modId;
        std::vector<StoredOption> options;
        std::unordered_map<std::string, std::size_t> optionIndices;
    };

    class Registry {
    public:
        void Initialize(const std::filesystem::path& configDirectory, Logger* logger);
        void Shutdown();

        bool RegisterSchema(const CoHModSDKConfigSchemaV1* schema);
        bool GetValue(const char* modId, const char* optionId, CoHModSDKConfigValueV1* outValue);
        bool SetValue(const char* modId, const char* optionId, const CoHModSDKConfigValueV1* value);
        bool EnumerateMods(CoHModSDKConfigModVisitor visitor, void* userData);
        bool EnumerateOptions(const char* modId, CoHModSDKConfigOptionVisitor visitor, void* userData);

    private:
        bool ValidateValue(const StoredOption& option, const CoHModSDKConfigValueV1& value) const;
        bool LoadPersistedValues(const std::string& modId, StoredModConfig& modConfig);
        bool SaveModConfig(const StoredModConfig& modConfig);
        std::filesystem::path GetConfigPath(const std::string& modId) const;
        void LogWarning(const std::string& message) const;
        void LogInfo(const std::string& message) const;

        std::mutex mutex;
        std::unordered_map<std::string, StoredModConfig> modConfigs;
        std::filesystem::path configDirectory;
        Logger* logger = nullptr;
    };
}
