#include "../include/CoHModSDK.hpp"

#include "hooks/HookEngine.hpp"
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

    bool CreateHookImpl(void* targetFunction, void* detourFunction, void** originalFunction) {
        return HookEngine::CreateHook(targetFunction, detourFunction, originalFunction);
    }

    bool EnableHookImpl(void* targetFunction) {
        return HookEngine::EnableHook(targetFunction);
    }

    bool DisableHookImpl(void* targetFunction) {
        return HookEngine::DisableHook(targetFunction);
    }

    const CoHModSDKApiV1 kApi = {
        COHMODSDK_ABI_VERSION,
        sizeof(CoHModSDKApiV1),
        &GetRuntimeInfoImpl,
        &LogImpl,
        &ShowErrorImpl,
        &Runtime::FindPattern,
        &Runtime::PatchMemory,
        &CreateHookImpl,
        &EnableHookImpl,
        &DisableHookImpl,
        &Runtime::RegisterConfigSchema,
        &Runtime::GetConfigValue,
        &Runtime::SetConfigValue,
        &Runtime::EnumerateConfigMods,
        &Runtime::EnumerateConfigOptions,
    };
}

extern "C" bool CoHModSDKRuntime_Initialize(const CoHModSDKRuntimeInitV1* init) {
    return Runtime::Initialize(init);
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
    if ((outApi == nullptr) || (abiVersion != COHMODSDK_ABI_VERSION)) {
        return false;
    }

    *outApi = &kApi;
    return true;
}
