#include "../include/CoHModSDK.hpp"

#include "runtime/RuntimeState.hpp"

namespace {
    const CoHModSDKRuntimeInfoV1* GetRuntimeInfoImpl() {
        return Runtime::GetRuntimeInfo();
    }

    void LogImpl(const CoHModSDKModContextV1* modContext, CoHModSDKLogLevel level, const char* message) {
        Runtime::LogForMod(modContext, level, message);
    }

    void ShowErrorImpl(const CoHModSDKModContextV1* modContext, const char* message) {
        Runtime::ShowModError(modContext, message);
    }

    std::optional<std::uintptr_t> FindPatternImpl(const char* moduleName, const char* signature) {
        return Runtime::FindPattern(moduleName, signature);
    }

    void PatchMemoryImpl(void* destination, const void* source, std::size_t size) {
        Runtime::PatchMemory(destination, source, size);
    }

    bool CreateHookImpl(void* targetFunction, void* detourFunction, void** originalFunction) {
        return Runtime::GetState().hookEngine.CreateHook(targetFunction, detourFunction, originalFunction);
    }

    bool RegisterConfigSchemaImpl(const CoHModSDKConfigSchemaV1* schema) {
        return Runtime::GetState().configRegistry.RegisterSchema(schema);
    }

    bool GetConfigValueImpl(const char* modId, const char* optionId, CoHModSDKConfigValueV1* outValue) {
        return Runtime::GetState().configRegistry.GetValue(modId, optionId, outValue);
    }

    bool SetConfigValueImpl(const char* modId, const char* optionId, const CoHModSDKConfigValueV1* value) {
        return Runtime::GetState().configRegistry.SetValue(modId, optionId, value);
    }

    bool EnumerateConfigModsImpl(CoHModSDKConfigModVisitor visitor, void* userData) {
        return Runtime::GetState().configRegistry.EnumerateMods(visitor, userData);
    }

    bool EnumerateConfigOptionsImpl(const char* modId, CoHModSDKConfigOptionVisitor visitor, void* userData) {
        return Runtime::GetState().configRegistry.EnumerateOptions(modId, visitor, userData);
    }

    bool GetConfigModInfoImpl(const char* modId, CoHModSDKConfigModInfoV1* outInfo) {
        return Runtime::GetRegisteredModInfo(modId, outInfo);
    }

    const CoHModSDKApiV1 kApi = {
        COHMODSDK_ABI_VERSION,
        sizeof(CoHModSDKApiV1),
        &GetRuntimeInfoImpl,
        &LogImpl,
        &ShowErrorImpl,
        &FindPatternImpl,
        &PatchMemoryImpl,
        &CreateHookImpl,
        &RegisterConfigSchemaImpl,
        &GetConfigValueImpl,
        &SetConfigValueImpl,
        &EnumerateConfigModsImpl,
        &EnumerateConfigOptionsImpl,
        &GetConfigModInfoImpl
    };
}

extern "C" bool CoHModSDKRuntime_Initialize(const CoHModSDKRuntimeInitV1* init) {
    return Runtime::Initialize(init);
}

extern "C" void CoHModSDKRuntime_EnableAllHooks() {
    Runtime::GetState().hookEngine.EnableAllHooks();
}

extern "C" void CoHModSDKRuntime_Shutdown() {
    Runtime::Shutdown();
}

extern "C" bool CoHModSDKRuntime_RegisterMod(HMODULE modHandle, const CoHModSDKModuleV1* module, const CoHModSDKModContextV1** outContext) {
    return Runtime::RegisterMod(modHandle, module, outContext);
}

extern "C" void CoHModSDKRuntime_UnregisterMod(HMODULE modHandle) {
    Runtime::UnregisterMod(modHandle);
}

extern "C" bool CoHModSDK_GetApi(std::uint32_t abiVersion, const CoHModSDKApiV1** outApi) {
    if ((outApi == nullptr) || (abiVersion > COHMODSDK_ABI_VERSION)) {
        return false;
    }

    *outApi = &kApi;
    return true;
}
