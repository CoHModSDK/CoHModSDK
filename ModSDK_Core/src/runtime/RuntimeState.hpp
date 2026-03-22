#pragma once

#include <Windows.h>

#include "../../include/CoHModSDK.hpp"

class Logger;

namespace Runtime {
    bool Initialize(const CoHModSDKRuntimeInitV1* init);
    void Shutdown();

    const CoHModSDKRuntimeInfoV1* GetRuntimeInfo();
    Logger& GetLogger();
    void LogForMod(const CoHModSDKModContextV1* modContext, CoHModSDKLogLevel level, const char* message);

    std::uintptr_t FindPattern(const char* moduleName, const char* signature, bool reportError);
    void PatchMemory(void* destination, const void* source, std::size_t size);
    void ShowModError(const CoHModSDKModContextV1* modContext, const char* message);

    bool RegisterConfigSchema(const CoHModSDKConfigSchemaV1* schema);
    bool GetConfigValue(const char* modId, const char* optionId, CoHModSDKConfigValueV1* outValue);
    bool SetConfigValue(const char* modId, const char* optionId, const CoHModSDKConfigValueV1* value);
    bool EnumerateConfigOptions(const char* modId, CoHModSDKConfigOptionVisitor visitor, void* userData);
    bool RegisterMod(HMODULE modHandle, const CoHModSDKModuleV1* module, const CoHModSDKModContextV1** outContext);
    void UnregisterMod(HMODULE modHandle);
}
