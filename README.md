# CoHModSDK

## 📜 Description

CoHModSDK is a lightweight SDK and loader for `Company of Heroes Relaunch`.

It is intended to make SDK-based mods easier to build and easier to install by
providing:

- a small modding SDK for native DLL mods
- a `WW2Mod.dll` wrapper loader
- a simple `mods/` loading workflow

## 📦 What Is Included

Releases are split into two parts:

- `CoHModSDK_Core`
  - for mod authors
  - contains `CoHModSDK.lib` and `CoHModSDK.hpp`
- `CoHModSDK_Loader`
  - for game installation
  - contains the wrapper `WW2Mod.dll`

## 🔧 Installing the Loader

The loader is designed to sit in the game directory as `WW2Mod.dll` and load
the original game DLL from `WW2Mod.original.dll`.

Expected layout:

```text
WW2Mod.dll
WW2Mod.original.dll
CoHModSDKLoader.ini
mods\
  YourMod.dll
```

Basic setup:

1. Rename the original game `WW2Mod.dll` to `WW2Mod.original.dll`
2. Place the CoHModSDK loader `WW2Mod.dll` into the game directory
3. Create `CoHModSDKLoader.ini` in the same directory
4. Put SDK mod DLLs into the `mods` directory

Example `CoHModSDKLoader.ini`:

```ini
# One DLL name per line
MyFirstSDKMod.dll
AnotherMod.dll
```

The loader writes a log file to:

```text
mods/logs/sdk-loader.log
```

## 📖 Documentation

Developer-focused documentation is in [`docs/development.md`](docs/development.md).

That includes:

- building the solution
- repository layout
- SDK exports and public API
- writing SDK mods
- current technical limitations

## 📄 License

See [LICENSE](LICENSE).

Important note:

- Independent mods using the project through its public interfaces are not required to use the repository license.
- Modified versions of this project itself are covered by the project license and its additional permissions.
