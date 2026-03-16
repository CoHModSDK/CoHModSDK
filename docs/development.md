# Development

This document covers the developer-facing side of CoHModSDK.

## Projects

The repository contains two Visual Studio projects:

- `ModSDK_Core`
  - static library for SDK mods
  - public header: `ModSDK_Core/include/CoHModSDK.hpp`
- `ModSDK_Loader`
  - `WW2Mod.dll` wrapper / proxy
  - loads `WW2Mod.original.dll`
  - loads mod DLLs listed in `CoHModSDKLoader.ini`

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
ModSDK_Core/
  include/CoHModSDK.hpp
  src/

ModSDK_Loader/
  src/
  res/
```

## Writing SDK Mods

SDK mods link against `CoHModSDK.lib` and include `CoHModSDK.hpp`.

The loader looks for these optional exports:

- `OnSDKLoad`
- `OnGameStart`
- `OnGameShutdown`
- `GetModName`
- `GetModVersion`
- `GetModAuthor`

Minimal example:

```cpp
#include "CoHModSDK.hpp"

extern "C" __declspec(dllexport) void OnSDKLoad() {
    auto gameModule = ModSDK::Memory::GetGameModuleHandle();
    auto target = ModSDK::Memory::FindPattern(
        gameModule,
        "55 8B EC",
        true
    );

    (void)target;
}

extern "C" __declspec(dllexport) void OnGameStart() {}
extern "C" __declspec(dllexport) void OnGameShutdown() {}

extern "C" __declspec(dllexport) const char* GetModName() {
    return "Example Mod";
}

extern "C" __declspec(dllexport) const char* GetModVersion() {
    return "1.0.0";
}

extern "C" __declspec(dllexport) const char* GetModAuthor() {
    return "Tosox";
}
```

## Public SDK API

Current public API groups:

- `ModSDK::Memory`
  - `GetGameModuleHandle()`
  - `FindPattern(...)`
  - `PatchMemory(...)`
- `ModSDK::Hooks`
  - `CreateHook(...)`
  - `EnableHook(...)`
  - `EnableAllHooks()`
  - `DisableHook(...)`
  - `DisableAllHooks()`

`GetGameModuleHandle()` prefers `WW2Mod.original.dll` and falls back to
`WW2Mod.dll`.

## Releases

The release workflow produces two archives:

- `CoHModSDK_Core-<version>.zip`
  - `CoHModSDK.lib`
  - `include/CoHModSDK.hpp`
- `CoHModSDK_Loader-<version>.zip`
  - `WW2Mod.dll`

## Technical Notes

- The hook engine is `x86` only.
- Hook creation is intentionally conservative and can fail on unsupported
  function prologues.
- The loader currently expects the wrapper/original split:
  - `WW2Mod.dll`
  - `WW2Mod.original.dll`
- The loader reads mod names from `CoHModSDKLoader.ini`.
