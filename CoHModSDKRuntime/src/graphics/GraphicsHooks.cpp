#include "GraphicsHooks.hpp"

#include <Windows.h>

#include <mutex>
#include <vector>

#include "../runtime/RuntimeState.hpp"

namespace {
    struct D3D9Callbacks {
        CoHModSDKD3D9CreateDevicePreFn pre = nullptr;
        CoHModSDKD3D9CreateDevicePostFn post = nullptr;
    };

    struct DXGICallbacks {
        CoHModSDKDXGICreateSwapChainPreFn pre = nullptr;
        CoHModSDKDXGICreateSwapChainPostFn post = nullptr;
    };

    std::mutex gMutex;
    std::vector<D3D9Callbacks> gD3D9Callbacks;
    std::vector<DXGICallbacks> gDXGICallbacks;

    using Direct3DCreate9Fn = IDirect3D9*(WINAPI*)(UINT);
    using D3D9CreateDeviceFn = HRESULT(__stdcall*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
    using CreateDXGIFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
    using DXGIFactoryCreateSwapChainFn = HRESULT(__stdcall*)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);

    Direct3DCreate9Fn oFnDirect3DCreate9 = nullptr;
    D3D9CreateDeviceFn oFnD3D9CreateDevice = nullptr;
    CreateDXGIFactoryFn oFnCreateDXGIFactory = nullptr;
    DXGIFactoryCreateSwapChainFn oFnDxgiCreateSwapChain = nullptr;

    HRESULT __stdcall HookedD3D9CreateDevice(IDirect3D9* _this, UINT adapter, D3DDEVTYPE deviceType, HWND window,
        DWORD flags, D3DPRESENT_PARAMETERS* params, IDirect3DDevice9** device) {
        {
            std::scoped_lock lock(gMutex);
            for (const D3D9Callbacks& cb : gD3D9Callbacks) {
                if ((cb.pre != nullptr) && !cb.pre(_this, &adapter, &deviceType, &window, &flags, params)) {
                    return E_ABORT;
                }
            }
        }

        const HRESULT result = oFnD3D9CreateDevice(_this, adapter, deviceType, window, flags, params, device);

        {
            std::scoped_lock lock(gMutex);
            for (const D3D9Callbacks& cb : gD3D9Callbacks) {
                if (cb.post != nullptr) {
                    cb.post(_this, adapter, deviceType, window, flags, params, result, (device != nullptr) ? *device : nullptr);
                }
            }
        }

        return result;
    }

    IDirect3D9* WINAPI HookedDirect3DCreate9(UINT version) {
        IDirect3D9* direct3D = oFnDirect3DCreate9(version);
        if (direct3D != nullptr) {
            Runtime::GetState().hookEngine.CreateHook(
                ModSDK::Memory::GetVTableEntry(direct3D, 16u),
                reinterpret_cast<void*>(&HookedD3D9CreateDevice),
                reinterpret_cast<void**>(&oFnD3D9CreateDevice));
        }
        return direct3D;
    }

    HRESULT __stdcall HookedDXGIFactoryCreateSwapChain(IDXGIFactory* _this, IUnknown* device,
        DXGI_SWAP_CHAIN_DESC* description, IDXGISwapChain** swapChain) {
        {
            std::scoped_lock lock(gMutex);
            for (const DXGICallbacks& cb : gDXGICallbacks) {
                if ((cb.pre != nullptr) && !cb.pre(_this, &device, description)) {
                    return E_ABORT;
                }
            }
        }

        const HRESULT result = oFnDxgiCreateSwapChain(_this, device, description, swapChain);

        {
            std::scoped_lock lock(gMutex);
            for (const DXGICallbacks& cb : gDXGICallbacks) {
                if (cb.post != nullptr) {
                    cb.post(_this, device, description, result, (swapChain != nullptr) ? *swapChain : nullptr);
                }
            }
        }

        return result;
    }

    HRESULT WINAPI HookedCreateDXGIFactory(REFIID riid, void** factory) {
        const HRESULT result = oFnCreateDXGIFactory(riid, factory);
        if (SUCCEEDED(result) && (factory != nullptr) && (*factory != nullptr)) {
            Runtime::GetState().hookEngine.CreateHook(
                ModSDK::Memory::GetVTableEntry(*factory, 10u),
                reinterpret_cast<void*>(&HookedDXGIFactoryCreateSwapChain),
                reinterpret_cast<void**>(&oFnDxgiCreateSwapChain));
        }
        return result;
    }
}

namespace GraphicsHooks {
    void Initialize() {
        HookEngine& hookEngine = Runtime::GetState().hookEngine;

        HMODULE hD3D9 = LoadLibraryA("d3d9.dll");
        if (hD3D9 != nullptr) {
            void* tDirect3DCreate9 = ModSDK::Memory::ResolveExport<void*>(hD3D9, "Direct3DCreate9");
            if (tDirect3DCreate9 != nullptr) {
                hookEngine.CreateHook(
                    tDirect3DCreate9,
                    reinterpret_cast<void*>(&HookedDirect3DCreate9),
                    reinterpret_cast<void**>(&oFnDirect3DCreate9));
            }
        }

        HMODULE hDxgi = LoadLibraryA("dxgi.dll");
        if (hDxgi != nullptr) {
            void* tCreateDXGIFactory = ModSDK::Memory::ResolveExport<void*>(hDxgi, "CreateDXGIFactory");
            if (tCreateDXGIFactory != nullptr) {
                hookEngine.CreateHook(
                    tCreateDXGIFactory,
                    reinterpret_cast<void*>(&HookedCreateDXGIFactory),
                    reinterpret_cast<void**>(&oFnCreateDXGIFactory));
            }
        }
    }

    void Shutdown() {
        std::scoped_lock lock(gMutex);
        gD3D9Callbacks.clear();
        gDXGICallbacks.clear();
    }

    bool RegisterD3D9CreateDevice(CoHModSDKD3D9CreateDevicePreFn pre, CoHModSDKD3D9CreateDevicePostFn post) {
        if ((pre == nullptr) && (post == nullptr)) {
            return false;
        }
        std::scoped_lock lock(gMutex);
        gD3D9Callbacks.push_back({pre, post});
        return true;
    }

    bool RegisterDXGICreateSwapChain(CoHModSDKDXGICreateSwapChainPreFn pre, CoHModSDKDXGICreateSwapChainPostFn post) {
        if ((pre == nullptr) && (post == nullptr)) {
            return false;
        }
        std::scoped_lock lock(gMutex);
        gDXGICallbacks.push_back({pre, post});
        return true;
    }
}
