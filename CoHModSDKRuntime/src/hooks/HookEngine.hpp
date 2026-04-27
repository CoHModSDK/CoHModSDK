#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

class HookEngine {
public:
    ~HookEngine();

    bool CreateHook(void* targetFunction, void* detourFunction, void** originalFunction);
    bool EnableHook(void* targetFunction);
    bool EnableAllHooks();
    bool DisableHook(void* targetFunction);
    bool DisableAllHooks();

    std::optional<std::uintptr_t> FindInOriginalBytes(const char* signature);

private:
    static constexpr std::size_t kMaxOriginalByteCount = 64;

    struct HookEntry {
        std::uint8_t* target = nullptr;
        std::uint8_t* detour = nullptr;
        std::uint8_t* trampoline = nullptr;
        std::array<std::uint8_t, kMaxOriginalByteCount> originalBytes = {};
        std::size_t patchLength = 0;
        std::size_t storedBytes = 0;
        bool enabled = false;
    };

    HookEntry* FindHookEntry(void* targetFunction);
    bool EnableHookInternal(HookEntry& hook);
    bool DisableHookInternal(HookEntry& hook);
    bool BuildTrampoline(HookEntry& hook);

    std::mutex mutex;
    std::vector<HookEntry> hooks;
    bool allHooksEnabled = false;
};
