#include "OriginalDll.hpp"

#include <Windows.h>
#include <filesystem>

#include "LoaderRuntime.hpp"
#include "exports/ProxyExports.hpp"

namespace {
    HMODULE g_originalWw2ModDll = nullptr;

    void ResolveOriginalExports() {
        const FARPROC mutexCtor = GetProcAddress(g_originalWw2ModDll, "??0_Mutex@std@@QAE@W4_Uninitialized@1@@Z");
        const FARPROC initLocksAssign = GetProcAddress(g_originalWw2ModDll, "??4_Init_locks@std@@QAEAAV01@ABV01@@Z");
        const auto getDllInterface = reinterpret_cast<Loader::GetDllInterfaceFunc>(GetProcAddress(g_originalWw2ModDll, "GetDllInterface"));
        const auto getDllVersion = reinterpret_cast<Loader::GetDllVersionFunc>(GetProcAddress(g_originalWw2ModDll, "GetDllVersion"));

        if ((mutexCtor == nullptr) || (initLocksAssign == nullptr) || (getDllInterface == nullptr) || (getDllVersion == nullptr)) {
            Loader::FailFast("WW2Mod.original.dll is missing one or more required exports");
        }

        Loader::SetStdMutexCtorExportTarget(mutexCtor);
        Loader::SetStdInitLocksAssignExportTarget(initLocksAssign);
        Loader::SetGetDllInterfaceExportTarget(getDllInterface);
        Loader::SetGetDllVersionExportTarget(getDllVersion);
    }
}

namespace Loader {
    void LoadOriginalDll() {
        const std::filesystem::path originalDllPath = GetRelativePath("WW2Mod.original.dll");
        g_originalWw2ModDll = LoadLibraryA(originalDllPath.string().c_str());
        if (!g_originalWw2ModDll) {
            FailFast("Failed to load WW2Mod.original.dll");
        }

        ResolveOriginalExports();
    }
}
