#include <Windows.h>

#include "LoaderRuntime.hpp"

BOOL APIENTRY DllMain(HMODULE hModule, unsigned long attachReason, void* reserved) {
    switch (attachReason) {
    case DLL_PROCESS_ATTACH:
        Loader::SetModuleHandle(hModule);
        DisableThreadLibraryCalls(hModule);
        Loader::Initialize();
        break;
    case DLL_PROCESS_DETACH:
        Loader::Shutdown();
        break;
    }

    return TRUE;
}
