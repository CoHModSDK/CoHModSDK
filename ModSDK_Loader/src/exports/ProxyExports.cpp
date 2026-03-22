#include "exports/ProxyExports.hpp"

#include "../LoaderRuntime.hpp"

namespace {
    FARPROC g_stdMutexCtorExport = nullptr;
    FARPROC g_stdInitLocksAssignExport = nullptr;
    Loader::GetDllInterfaceFunc g_getDllInterfaceExport = nullptr;
    Loader::GetDllVersionFunc g_getDllVersionExport = nullptr;
}

namespace Loader {
    void SetStdMutexCtorExportTarget(FARPROC target) {
        g_stdMutexCtorExport = target;
    }

    void SetStdInitLocksAssignExportTarget(FARPROC target) {
        g_stdInitLocksAssignExport = target;
    }

    void SetGetDllInterfaceExportTarget(GetDllInterfaceFunc target) {
        g_getDllInterfaceExport = target;
    }

    void SetGetDllVersionExportTarget(GetDllVersionFunc target) {
        g_getDllVersionExport = target;
    }
}

extern "C" __declspec(naked) void ForwardStdMutexCtor() {
    __asm {
        jmp dword ptr [g_stdMutexCtorExport]
    }
}

extern "C" __declspec(naked) void ForwardStdInitLocksAssign() {
    __asm {
        jmp dword ptr [g_stdInitLocksAssignExport]
    }
}

extern "C" int ExportedGetDllInterface() {
    Loader::EnsureInitialized();
    return g_getDllInterfaceExport();
}

extern "C" int ExportedGetDllVersion() {
    Loader::EnsureInitialized();
    return g_getDllVersionExport();
}
