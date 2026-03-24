# Development

This document covers the developer-facing side of CoHModSDK.

## Projects

The repository contains two Visual Studio projects:

- `CoHModSDKRuntime`
  - builds the required shared runtime: `CoHModSDKRuntime.dll`
  - also produces the import library: `CoHModSDK.lib`
  - public header: `CoHModSDKRuntime/include/CoHModSDK.hpp`
- `CoHModSDKLoader`
  - builds `CoHModSDKLoader.dll`
  - used by mods that set `DllName = CoHModSDKLoader`
  - loads the shared runtime first, then loads mod DLLs listed in `CoHModSDKLoader.ini`

## Requirements

- Visual Studio 2022
- MSVC `v143`
- Windows SDK

## Building

Build the solution:

```powershell
msbuild /m /p:Configuration=Release /p:Platform=x86 .
```

The GitHub workflow uses the same `Release|x86` configuration.

## Repository Layout

```text
CoHModSDKRuntime/
  include/CoHModSDK.hpp
  src/

CoHModSDKLoader/
  src/
  res/
```

## Runtime Model

- `CoHModSDKLoader.dll` is the bootstrap/proxy DLL.
- `CoHModSDKRuntime.dll` is required infrastructure and owns shared SDK state.
- Mods link against `CoHModSDK.lib`, include `CoHModSDK.hpp`, and export a module descriptor through `CoHMod_GetModule`.
- The loader injects an opaque per-mod runtime context before `OnInitialize`; `COHMODSDK_EXPORT_MODULE(...)` exports the required context setter automatically.
- Config data is stored under `mods/config/<modId>.json`.

## Writing SDK Mods

SDK mods link against `CoHModSDK.lib`, include `CoHModSDK.hpp`, and export `CoHMod_GetModule`.

Minimal example:

```cpp
#include "CoHModSDK.hpp"

namespace {
    bool OnInitialize() {
        auto address = ModSDK::Memory::FindPattern("WW2Mod.dll", "55 8B EC", true);
        (void)address;
        return true;
    }

    bool OnModsLoaded() {
        return true;
    }

    void OnShutdown() {}

    const CoHModSDKModuleV1 kModule = {
        COHMODSDK_ABI_VERSION,
        sizeof(CoHModSDKModuleV1),
        "de.tosox.examplemod",
        "Example Mod",
        "1.0.0",
        "Tosox",
        &OnInitialize,
        &OnModsLoaded,
        &OnShutdown,
    };
}

COHMODSDK_EXPORT_MODULE(kModule);
```

## Debugging Mods

SDK mods are DLLs, so you debug them by launching or attaching to the game
process, not by trying to run the DLL directly.

Recommended Visual Studio setup for a mod project:

1. Build the mod in `Debug|Win32` so you get both the DLL and the PDB.
2. Copy the mod DLL and PDB into the game's `mods/` directory.
3. Make sure the game directory also contains:
   - `CoHModSDKLoader.dll`
   - `CoHModSDKRuntime.dll`
   - `CoHModSDKLoader.ini`
4. Make sure the target `.module` file uses `DllName = CoHModSDKLoader`.
5. In the mod project's `Debugging` properties, set:
   - `Command` = the game executable
   - `Working Directory` = the game directory
   - `Command Arguments` = your normal game launch arguments, if any
6. Start debugging with `F5`.

Current load order for an SDK mod:

1. The game loads `CoHModSDKLoader.dll`.
2. The loader reads `CoHModSDKLoader.ini`.
3. The loader `LoadLibrary`s the mod DLL.
4. Windows executes the mod's `DllMain(..., DLL_PROCESS_ATTACH, ...)`.
5. The loader calls `CoHMod_GetModule(...)`.
6. The loader calls `CoHMod_SetContext(...)`.
7. The loader calls `OnInitialize()`.
8. After all listed mods are initialized, the loader calls `OnModsLoaded()`.

Practical debugging guidance:

- Put breakpoints in `OnInitialize()`, `OnModsLoaded()`, hook callbacks, and config callbacks.
- Keep `DllMain` minimal. It runs before the runtime context is injected and is the wrong place for most mod logic.
- Use `Debug > Windows > Modules` in Visual Studio to confirm that the mod DLL is loaded and that its PDB has been found.
- If symbols do not load automatically, load them manually from the `Modules` window.
- If you need to break immediately when the mod starts, temporarily add `__debugbreak();` at the start of `OnInitialize()`.
- If you attach to an already running game process, early startup breakpoints will not hit until the game is restarted.

## Public SDK API

Current public API groups:

- `ModSDK::Runtime`
  - `GetInfo()`
  - `Log(...)`
- `ModSDK::Dialogs`
  - `ShowError(...)`
- `ModSDK::Memory`
  - `FindPattern(...)`
  - `PatchMemory(...)`
- `ModSDK::Hooks`
  - `CreateHook(...)`
  - `EnableHook(...)`
  - `DisableHook(...)`
- `ModSDK::Config`
  - `RegisterSchema(...)`
  - `GetValue(...)`
  - `SetValue(...)`
  - `EnumerateMods(...)`
  - `EnumerateOptions(...)`
  - `MakeBoolValue(...)`
  - `MakeIntValue(...)`
  - `MakeFloatValue(...)`
  - `MakeEnumValue(...)`

Internally, the runtime boundary is a versioned `extern "C"` API table exposed by `CoHModSDKRuntime.dll`.
Mod identity is carried through an explicit runtime-owned context handle, not by inferring the caller from the return address.

## Releases

The release workflow should produce two archives:

- `CoHModSDK-<version>.zip`
  - `CoHModSDKLoader.dll`
  - `CoHModSDKRuntime.dll`
  - `CoHModSDKLoader.ini`
- `CoHModSDK-Dev-<version>.zip`
  - `CoHModSDK.lib`
  - `include/CoHModSDK.hpp`

## Technical Notes

- The hook engine is `x86` only.
- Hook creation is intentionally conservative and can fail on unsupported function prologues.
- SDK-enabled mods are expected to use `DllName = CoHModSDKLoader`.
- The loader reads mod names from `CoHModSDKLoader.ini`.
- `CoHModSDKRuntime.dll` must be present beside the loader at runtime.
