#pragma once

#include "../../include/CoHModSDKGraphics.hpp"

namespace GraphicsHooks {
    void Initialize();
    void Shutdown();

    bool RegisterD3D9CreateDevice(CoHModSDKD3D9CreateDevicePreFn pre, CoHModSDKD3D9CreateDevicePostFn post);
    bool RegisterDXGICreateSwapChain(CoHModSDKDXGICreateSwapChainPreFn pre, CoHModSDKDXGICreateSwapChainPostFn post);
}
