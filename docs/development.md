# Development

This document covers the developer-facing side of CoHModSDK.

## Projects

The repository contains two Visual Studio projects:

- `ModSDK_Core`
  - static library for SDK mods
  - public header: `ModSDK_Core/include/CoHModSDK.hpp`
- `ModSDK_Loader`
  - `CoHModSDKLoader.dll`
  - used by mods that set `DllName = CoHModSDKLoader`
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
  - `DisableHook(...)`

## Releases

The release workflow produces two archives:

- `CoHModSDK_Core-<version>.zip`
  - `CoHModSDK.lib`
  - `include/CoHModSDK.hpp`
- `CoHModSDK_Loader-<version>.zip`
  - `CoHModSDKLoader.dll`

## Technical Notes

- The hook engine is `x86` only.
- Hook creation is intentionally conservative and can fail on unsupported
  function prologues.
- SDK-enabled mods are expected to use `DllName = CoHModSDKLoader`.
- The loader reads mod names from `CoHModSDKLoader.ini`.
