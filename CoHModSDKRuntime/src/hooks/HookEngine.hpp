#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

class HookEngine {
public:
    bool CreateHook(void* targetFunction, void* detourFunction, void** originalFunction);
    bool EnableHook(void* targetFunction);
    bool EnableAllHooks();
    bool DisableHook(void* targetFunction);
    bool DisableAllHooks();

private:
    static constexpr std::size_t kMaxOriginalByteCount = 32;

    struct HookEntry {
        std::uint8_t* target = nullptr;
        std::uint8_t* detour = nullptr;
        std::uint8_t* trampoline = nullptr;
        std::array<std::uint8_t, kMaxOriginalByteCount> originalBytes = {};
        std::size_t patchLength = 0;
        bool enabled = false;
    };

    HookEntry* FindHookEntry(void* targetFunction);
    bool EnableHookInternal(HookEntry& hook);
    bool DisableHookInternal(HookEntry& hook);
    bool BuildTrampoline(HookEntry& hook);

    std::mutex mutex;
    std::vector<HookEntry> hooks;
};
