#include "exports/ProxyExports.hpp"

#include "../core/Loader.hpp"

namespace {
    Loader::GetDllInterfaceFn oFnGetDllInterface = nullptr;
    Loader::GetDllVersionFn oFnGetDllVersion = nullptr;
}

namespace Loader {
    void SetGetDllInterfaceExportTarget(GetDllInterfaceFn target) {
        oFnGetDllInterface = target;
    }

    void SetGetDllVersionExportTarget(GetDllVersionFn target) {
        oFnGetDllVersion = target;
    }
}

extern "C" __declspec(dllexport) int GetDllInterface() {
    Loader::EnsureInitialized();
    return oFnGetDllInterface();
}

extern "C" __declspec(dllexport) int GetDllVersion() {
    Loader::EnsureInitialized();
    return oFnGetDllVersion();
}
