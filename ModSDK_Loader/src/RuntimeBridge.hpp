#pragma once

#include <Windows.h>

#include "../../ModSDK_Core/include/CoHModSDK.hpp"

namespace Loader {
    void LoadRuntime();
    void ShutdownRuntime();
    bool RegisterModWithRuntime(HMODULE modHandle, const CoHModSDKModuleV1* module, const CoHModSDKModContextV1** outContext);
    void UnregisterModWithRuntime(HMODULE modHandle);
}
