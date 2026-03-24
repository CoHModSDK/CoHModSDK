#include "ConfigRegistry.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../utils/Logger.hpp"

namespace Runtime::Config {
    struct ParsedValue {
        enum class Kind {
            Bool,
            Number,
        };

        Kind kind = Kind::Bool;
        bool boolValue = false;
        double numberValue = 0.0;
    };

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

    namespace {
        struct PendingCallback {
            std::string modId;
            std::string optionId;
            CoHModSDKConfigValueV1 value = {};
            CoHModSDKConfigChangedCallback callback = nullptr;
            void* userData = nullptr;
        };

        std::mutex g_registryMutex;
        std::unordered_map<std::string, StoredModConfig> g_modConfigs;

        class JsonParser {
        public:
            explicit JsonParser(std::string_view text)
                : text(text) {
            }

            bool ParseObject(std::unordered_map<std::string, ParsedValue>& outValues) {
                SkipWhitespace();
                if (!Consume('{')) {
                    return false;
                }

                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }

                while (position < text.size()) {
                    std::string key;
                    if (!ParseString(key)) {
                        return false;
                    }

                    SkipWhitespace();
                    if (!Consume(':')) {
                        return false;
                    }

                    SkipWhitespace();
                    ParsedValue value = {};
                    if (!ParseValue(value)) {
                        return false;
                    }

                    outValues.emplace(std::move(key), value);

                    SkipWhitespace();
                    if (Consume('}')) {
                        return true;
                    }

                    if (!Consume(',')) {
                        return false;
                    }

                    SkipWhitespace();
                }

                return false;
            }

        private:
            bool ParseValue(ParsedValue& outValue) {
                if (MatchLiteral("true")) {
                    outValue.kind = ParsedValue::Kind::Bool;
                    outValue.boolValue = true;
                    return true;
                }

                if (MatchLiteral("false")) {
                    outValue.kind = ParsedValue::Kind::Bool;
                    outValue.boolValue = false;
                    return true;
                }

                return ParseNumber(outValue);
            }

            bool ParseNumber(ParsedValue& outValue) {
                const std::size_t start = position;
                if ((position < text.size()) && ((text[position] == '-') || (text[position] == '+'))) {
                    ++position;
                }

                bool seenDigit = false;
                while ((position < text.size()) && std::isdigit(static_cast<unsigned char>(text[position]))) {
                    ++position;
                    seenDigit = true;
                }

                if ((position < text.size()) && (text[position] == '.')) {
                    ++position;
                    while ((position < text.size()) && std::isdigit(static_cast<unsigned char>(text[position]))) {
                        ++position;
                        seenDigit = true;
                    }
                }

                if (!seenDigit) {
                    return false;
                }

                const std::string numberText(text.substr(start, position - start));
                char* end = nullptr;
                errno = 0;
                const double parsedNumber = std::strtod(numberText.c_str(), &end);
                if ((end == nullptr) || (*end != '\0') || (errno != 0)) {
                    return false;
                }

                outValue.kind = ParsedValue::Kind::Number;
                outValue.numberValue = parsedNumber;
                return true;
            }

            bool ParseString(std::string& outValue) {
                if (!Consume('"')) {
                    return false;
                }

                outValue.clear();
                while (position < text.size()) {
                    const char current = text[position++];
                    if (current == '"') {
                        return true;
                    }

                    if (current == '\\') {
                        if (position >= text.size()) {
                            return false;
                        }

                        const char escaped = text[position++];
                        switch (escaped) {
                        case '\\':
                        case '"':
                        case '/':
                            outValue.push_back(escaped);
                            break;
                        case 'b':
                            outValue.push_back('\b');
                            break;
                        case 'f':
                            outValue.push_back('\f');
                            break;
                        case 'n':
                            outValue.push_back('\n');
                            break;
                        case 'r':
                            outValue.push_back('\r');
                            break;
                        case 't':
                            outValue.push_back('\t');
                            break;
                        default:
                            return false;
                        }
                        continue;
                    }

                    outValue.push_back(current);
                }

                return false;
            }

            bool MatchLiteral(std::string_view literal) {
                if (text.substr(position, literal.size()) != literal) {
                    return false;
                }

                position += literal.size();
                return true;
            }

            bool Consume(char expected) {
                if ((position < text.size()) && (text[position] == expected)) {
                    ++position;
                    return true;
                }

                return false;
            }

            void SkipWhitespace() {
                while ((position < text.size()) && std::isspace(static_cast<unsigned char>(text[position]))) {
                    ++position;
                }
            }

        private:
            std::string_view text;
            std::size_t position = 0;
        };

        bool UsesNumericRange(const StoredOption& option) {
            return option.minValue < option.maxValue;
        }

        bool IsIntegralNumber(double value) {
            return std::floor(value) == value;
        }

        std::string EscapeJsonString(std::string_view value) {
            std::string escaped;
            escaped.reserve(value.size() + 8);

            for (const char current : value) {
                switch (current) {
                case '\\':
                    escaped += "\\\\";
                    break;
                case '"':
                    escaped += "\\\"";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '\t':
                    escaped += "\\t";
                    break;
                default:
                    escaped.push_back(current);
                    break;
                }
            }

            return escaped;
        }

        std::string SerializeValue(const CoHModSDKConfigValueV1& value) {
            std::ostringstream stream;
            stream.setf(std::ios::fixed, std::ios::floatfield);
            stream.precision(4);

            switch (value.type) {
            case CoHModSDKConfigType_Bool:
                return value.boolValue != 0 ? "true" : "false";
            case CoHModSDKConfigType_Int:
                return std::to_string(value.intValue);
            case CoHModSDKConfigType_Float:
                stream << value.floatValue;
                return stream.str();
            case CoHModSDKConfigType_Enum:
                return std::to_string(value.enumValue);
            default:
                return "null";
            }
        }

        void RefreshChoiceView(const StoredChoice& choice, CoHModSDKConfigChoiceV1& choiceView) {
            choiceView.value = choice.value;
            choiceView.valueId = choice.valueId.empty() ? nullptr : choice.valueId.c_str();
            choiceView.label = choice.label.empty() ? nullptr : choice.label.c_str();
        }

        void RefreshOptionView(StoredOption& option) {
            option.choiceViews.resize(option.choices.size());
            for (std::size_t index = 0; index < option.choices.size(); ++index) {
                RefreshChoiceView(option.choices[index], option.choiceViews[index]);
            }

            option.view.optionId = option.optionId.c_str();
            option.view.category = option.category.empty() ? nullptr : option.category.c_str();
            option.view.label = option.label.empty() ? nullptr : option.label.c_str();
            option.view.description = option.description.empty() ? nullptr : option.description.c_str();
            option.view.type = option.type;
            option.view.defaultValue = option.defaultValue;
            option.view.minValue = option.minValue;
            option.view.maxValue = option.maxValue;
            option.view.step = option.step;
            option.view.flags = option.flags;
            option.view.choices = option.choiceViews.empty() ? nullptr : option.choiceViews.data();
            option.view.choiceCount = static_cast<std::uint32_t>(option.choiceViews.size());
            option.view.onChanged = option.onChanged;
            option.view.userData = option.userData;
        }

        bool TryConvertParsedValue(const StoredOption& option, const ParsedValue& parsedValue, CoHModSDKConfigValueV1& outValue) {
            outValue = {};
            outValue.type = option.type;

            switch (option.type) {
            case CoHModSDKConfigType_Bool:
                if (parsedValue.kind == ParsedValue::Kind::Bool) {
                    outValue.boolValue = parsedValue.boolValue ? 1u : 0u;
                    return true;
                }

                if (parsedValue.kind == ParsedValue::Kind::Number) {
                    outValue.boolValue = parsedValue.numberValue != 0.0 ? 1u : 0u;
                    return true;
                }
                return false;
            case CoHModSDKConfigType_Int:
                if ((parsedValue.kind != ParsedValue::Kind::Number) || !IsIntegralNumber(parsedValue.numberValue)) {
                    return false;
                }
                outValue.intValue = static_cast<std::int32_t>(parsedValue.numberValue);
                return true;
            case CoHModSDKConfigType_Float:
                if (parsedValue.kind != ParsedValue::Kind::Number) {
                    return false;
                }
                outValue.floatValue = static_cast<float>(parsedValue.numberValue);
                return true;
            case CoHModSDKConfigType_Enum:
                if ((parsedValue.kind != ParsedValue::Kind::Number) || !IsIntegralNumber(parsedValue.numberValue)) {
                    return false;
                }
                outValue.enumValue = static_cast<std::int32_t>(parsedValue.numberValue);
                return true;
            default:
                return false;
            }
        }

        std::string SerializeConfigDocument(const StoredModConfig& modConfig) {
            std::ostringstream stream;
            stream << "{\n";

            for (std::size_t index = 0; index < modConfig.options.size(); ++index) {
                const StoredOption& option = modConfig.options[index];
                stream << "  \"" << EscapeJsonString(option.optionId) << "\": " << SerializeValue(option.currentValue);
                if ((index + 1u) < modConfig.options.size()) {
                    stream << ",";
                }
                stream << "\n";
            }

            stream << "}\n";
            return stream.str();
        }

        std::vector<PendingCallback> BuildCallbacks(const StoredModConfig& modConfig) {
            std::vector<PendingCallback> callbacks;
            callbacks.reserve(modConfig.options.size());

            for (const StoredOption& option : modConfig.options) {
                if (option.onChanged == nullptr) {
                    continue;
                }

                PendingCallback callback = {};
                callback.modId = modConfig.modId;
                callback.optionId = option.optionId;
                callback.value = option.currentValue;
                callback.callback = option.onChanged;
                callback.userData = option.userData;
                callbacks.push_back(std::move(callback));
            }

            return callbacks;
        }

        void InvokeCallbacks(const std::vector<PendingCallback>& callbacks) {
            for (const PendingCallback& callback : callbacks) {
                callback.callback(callback.modId.c_str(), callback.optionId.c_str(), &callback.value, callback.userData);
            }
        }
    }

    void Registry::Initialize(const std::filesystem::path& configDirectory, Logger* logger) {
        this->configDirectory = configDirectory;
        this->logger = logger;

        std::error_code error;
        std::filesystem::create_directories(this->configDirectory, error);
        (void)error;

        std::scoped_lock lock(g_registryMutex);
        g_modConfigs.clear();
    }

    void Registry::Shutdown() {
        std::scoped_lock lock(g_registryMutex);
        g_modConfigs.clear();
    }

    bool Registry::RegisterSchema(const CoHModSDKConfigSchemaV1* schema) {
        if ((schema == nullptr) || (schema->modId == nullptr) || (schema->options == nullptr) || (schema->optionCount == 0u)) {
            return false;
        }

        StoredModConfig modConfig = {};
        modConfig.modId = schema->modId;
        modConfig.options.reserve(schema->optionCount);

        for (std::uint32_t index = 0; index < schema->optionCount; ++index) {
            const CoHModSDKConfigOptionV1& sourceOption = schema->options[index];
            if (sourceOption.optionId == nullptr) {
                return false;
            }

            if (modConfig.optionIndices.contains(sourceOption.optionId)) {
                return false;
            }

            StoredOption option = {};
            option.optionId = sourceOption.optionId;
            option.category = sourceOption.category == nullptr ? std::string() : std::string(sourceOption.category);
            option.label = sourceOption.label == nullptr ? option.optionId : std::string(sourceOption.label);
            option.description = sourceOption.description == nullptr ? std::string() : std::string(sourceOption.description);
            option.type = sourceOption.type;
            option.defaultValue = sourceOption.defaultValue;
            option.currentValue = sourceOption.defaultValue;
            option.minValue = sourceOption.minValue;
            option.maxValue = sourceOption.maxValue;
            option.step = sourceOption.step;
            option.flags = sourceOption.flags;
            option.onChanged = sourceOption.onChanged;
            option.userData = sourceOption.userData;

            option.choices.reserve(sourceOption.choiceCount);
            for (std::uint32_t choiceIndex = 0; choiceIndex < sourceOption.choiceCount; ++choiceIndex) {
                const CoHModSDKConfigChoiceV1& sourceChoice = sourceOption.choices[choiceIndex];
                StoredChoice choice = {};
                choice.value = sourceChoice.value;
                choice.valueId = sourceChoice.valueId == nullptr ? std::string() : std::string(sourceChoice.valueId);
                choice.label = sourceChoice.label == nullptr ? std::string() : std::string(sourceChoice.label);
                option.choices.push_back(std::move(choice));
            }

            if (!ValidateValue(option, option.defaultValue)) {
                LogWarning("Rejected invalid default config value for " + modConfig.modId + "." + option.optionId);
                return false;
            }

            modConfig.optionIndices.emplace(option.optionId, modConfig.options.size());
            modConfig.options.push_back(std::move(option));
            RefreshOptionView(modConfig.options.back());
        }

        LoadPersistedValues(modConfig.modId, modConfig);

        std::vector<PendingCallback> callbacks;
        {
            std::scoped_lock lock(g_registryMutex);
            if (g_modConfigs.contains(modConfig.modId)) {
                LogWarning("Config schema for mod '" + modConfig.modId + "' was already registered");
                return false;
            }

            auto [iterator, inserted] = g_modConfigs.emplace(modConfig.modId, std::move(modConfig));
            if (!inserted) {
                return false;
            }

            callbacks = BuildCallbacks(iterator->second);
        }

        InvokeCallbacks(callbacks);
        LogInfo("Registered config schema for mod '" + std::string(schema->modId) + "'");
        return true;
    }

    bool Registry::GetValue(const char* modId, const char* optionId, CoHModSDKConfigValueV1* outValue) {
        if ((modId == nullptr) || (optionId == nullptr) || (outValue == nullptr)) {
            return false;
        }

        std::scoped_lock lock(g_registryMutex);
        const auto modIterator = g_modConfigs.find(modId);
        if (modIterator == g_modConfigs.end()) {
            return false;
        }

        const auto optionIterator = modIterator->second.optionIndices.find(optionId);
        if (optionIterator == modIterator->second.optionIndices.end()) {
            return false;
        }

        *outValue = modIterator->second.options[optionIterator->second].currentValue;
        return true;
    }

    bool Registry::SetValue(const char* modId, const char* optionId, const CoHModSDKConfigValueV1* value) {
        if ((modId == nullptr) || (optionId == nullptr) || (value == nullptr)) {
            return false;
        }

        PendingCallback callback = {};
        bool hasCallback = false;

        {
            std::scoped_lock lock(g_registryMutex);
            const auto modIterator = g_modConfigs.find(modId);
            if (modIterator == g_modConfigs.end()) {
                return false;
            }

            const auto optionIterator = modIterator->second.optionIndices.find(optionId);
            if (optionIterator == modIterator->second.optionIndices.end()) {
                return false;
            }

            StoredOption& option = modIterator->second.options[optionIterator->second];
            if (!ValidateValue(option, *value)) {
                return false;
            }

            option.currentValue = *value;
            if (!SaveModConfig(modIterator->second)) {
                return false;
            }

            if (option.onChanged != nullptr) {
                callback.modId = modIterator->second.modId;
                callback.optionId = option.optionId;
                callback.value = option.currentValue;
                callback.callback = option.onChanged;
                callback.userData = option.userData;
                hasCallback = true;
            }
        }

        if (hasCallback) {
            callback.callback(callback.modId.c_str(), callback.optionId.c_str(), &callback.value, callback.userData);
        }

        return true;
    }

    bool Registry::EnumerateMods(CoHModSDKConfigModVisitor visitor, void* userData) {
        if (visitor == nullptr) {
            return false;
        }

        std::scoped_lock lock(g_registryMutex);
        for (const auto& [modId, modConfig] : g_modConfigs) {
            if (!visitor(modId.c_str(), userData)) {
                break;
            }
        }

        return true;
    }

    bool Registry::EnumerateOptions(const char* modId, CoHModSDKConfigOptionVisitor visitor, void* userData) {
        if ((modId == nullptr) || (visitor == nullptr)) {
            return false;
        }

        std::scoped_lock lock(g_registryMutex);
        const auto modIterator = g_modConfigs.find(modId);
        if (modIterator == g_modConfigs.end()) {
            return false;
        }

        for (const StoredOption& option : modIterator->second.options) {
            if (!visitor(&option.view, &option.currentValue, userData)) {
                break;
            }
        }

        return true;
    }

    bool Registry::ValidateValue(const StoredOption& option, const CoHModSDKConfigValueV1& value) const {
        if (option.type != value.type) {
            return false;
        }

        switch (value.type) {
        case CoHModSDKConfigType_Bool:
            return true;
        case CoHModSDKConfigType_Int:
            if (UsesNumericRange(option) && ((static_cast<float>(value.intValue) < option.minValue) || (static_cast<float>(value.intValue) > option.maxValue))) {
                return false;
            }
            return true;
        case CoHModSDKConfigType_Float:
            if (UsesNumericRange(option) && ((value.floatValue < option.minValue) || (value.floatValue > option.maxValue))) {
                return false;
            }
            return true;
        case CoHModSDKConfigType_Enum:
            return std::any_of(
                option.choices.begin(),
                option.choices.end(),
                [&value](const StoredChoice& choice) {
                    return choice.value == value.enumValue;
                }
            );
        default:
            return false;
        }
    }

    bool Registry::LoadPersistedValues(const std::string& modId, StoredModConfig& modConfig) {
        const std::filesystem::path configPath = GetConfigPath(modId);
        std::ifstream input(configPath);
        if (!input.is_open()) {
            return true;
        }

        const std::string contents(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>()
        );

        std::unordered_map<std::string, ParsedValue> parsedValues;
        JsonParser parser(contents);
        if (!parser.ParseObject(parsedValues)) {
            LogWarning("Failed to parse config file '" + configPath.string() + "'");
            return false;
        }

        for (StoredOption& option : modConfig.options) {
            const auto valueIterator = parsedValues.find(option.optionId);
            if (valueIterator == parsedValues.end()) {
                continue;
            }

            CoHModSDKConfigValueV1 convertedValue = {};
            if (!TryConvertParsedValue(option, valueIterator->second, convertedValue) || !ValidateValue(option, convertedValue)) {
                LogWarning("Ignored invalid persisted config value for " + modId + "." + option.optionId);
                continue;
            }

            option.currentValue = convertedValue;
        }

        return true;
    }

    bool Registry::SaveModConfig(const StoredModConfig& modConfig) {
        const std::filesystem::path configPath = GetConfigPath(modConfig.modId);
        std::error_code error;
        std::filesystem::create_directories(configPath.parent_path(), error);
        if (error) {
            LogWarning("Failed to create config directory for '" + modConfig.modId + "'");
            return false;
        }

        std::ofstream output(configPath, std::ios::out | std::ios::trunc);
        if (!output.is_open()) {
            LogWarning("Failed to write config file '" + configPath.string() + "'");
            return false;
        }

        output << SerializeConfigDocument(modConfig);
        return true;
    }

    std::filesystem::path Registry::GetConfigPath(const std::string& modId) const {
        return configDirectory / (modId + ".json");
    }

    void Registry::LogWarning(const std::string& message) const {
        if (logger != nullptr) {
            logger->LogWarning(message);
        }
    }

    void Registry::LogInfo(const std::string& message) const {
        if (logger != nullptr) {
            logger->LogInfo(message);
        }
    }
}
