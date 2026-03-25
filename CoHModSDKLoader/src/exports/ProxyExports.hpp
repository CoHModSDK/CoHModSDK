#pragma once

namespace Loader {
    using GetDllInterfaceFn = int(*)();
    using GetDllVersionFn = int(*)();

    void SetGetDllInterfaceExportTarget(GetDllInterfaceFn target);
    void SetGetDllVersionExportTarget(GetDllVersionFn target);
}
