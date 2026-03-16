#pragma once

#include <Windows.h>

namespace Loader {
    using GetDllInterfaceFunc = int(*)();
    using GetDllVersionFunc = int(*)();

    void SetStdMutexCtorExportTarget(FARPROC target);
    void SetStdInitLocksAssignExportTarget(FARPROC target);
    void SetGetDllInterfaceExportTarget(GetDllInterfaceFunc target);
    void SetGetDllVersionExportTarget(GetDllVersionFunc target);
}
