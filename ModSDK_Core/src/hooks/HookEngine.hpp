#pragma once

namespace HookEngine {
    bool CreateHook(void* targetFunction, void* detourFunction, void** originalFunction);
    bool EnableHook(void* targetFunction);
    bool EnableAllHooks();
    bool DisableHook(void* targetFunction);
    bool DisableAllHooks();
}
