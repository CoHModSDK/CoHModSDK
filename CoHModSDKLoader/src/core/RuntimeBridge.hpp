#pragma once

#include <Windows.h>

#include "../../../CoHModSDKRuntime/include/CoHModSDK.hpp"

namespace Loader {
    void LoadRuntime();
    void EnableAllHooks();
    void ShutdownRuntime();
    bool RegisterModWithRuntime(HMODULE modHandle, const CoHModSDKModuleV1* module, const CoHModSDKModContextV1** outContext);
    void UnregisterModWithRuntime(HMODULE modHandle);
}
