#include <Windows.h>
#include <format>
#include <stdexcept>
#include <cstring>
#include <vector>

#include "../include/CoHModSDK.hpp"
#include "hooks/HookEngine.hpp"

namespace ModSDK {
    namespace Memory {
        namespace {
            constexpr const char* kOriginalGameModuleName = "WW2Mod.original.dll";
            constexpr const char* kWrappedGameModuleName = "WW2Mod.dll";

            std::string DescribeModule(HMODULE moduleHandle) {
                if (!moduleHandle) {
                    return "<null>";
                }

                char modulePath[MAX_PATH] = {};
                if (GetModuleFileNameA(moduleHandle, modulePath, MAX_PATH) == 0) {
                    return std::format("0x{:08X}", reinterpret_cast<std::uintptr_t>(moduleHandle));
                }

                return modulePath;
            }

            std::uintptr_t FindPatternInModule(HMODULE moduleHandle, const char* signature, bool reportError) {
                const std::string moduleDescription = DescribeModule(moduleHandle);
                if (!moduleHandle) {
                    if (reportError) {
                        std::string errorMessage = std::format("Unable to get a handle for module {}", moduleDescription);
                        MessageBoxA(nullptr, errorMessage.c_str(), "Error", MB_ICONERROR);
                        throw std::runtime_error(errorMessage);
                    }
                    return 0;
                }

                static auto patternToBytes = [](const char* pattern) {
                    std::vector<int> bytes;
                    char* start = const_cast<char*>(pattern);
                    char* end = const_cast<char*>(pattern) + std::strlen(pattern);

                    for (char* current = start; current < end; ++current) {
                        if (*current == '?') {
                            ++current;
                            if (*current == '?') {
                                ++current;
                            }
                            bytes.push_back(-1);
                        }
                        else {
                            bytes.push_back(std::strtoul(current, &current, 16));
                        }
                    }
                    return bytes;
                };

                PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)(moduleHandle);
                PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)(moduleHandle)+dosHeader->e_lfanew);

                std::size_t sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
                std::vector<int> patternBytes = patternToBytes(signature);
                std::uint8_t* scanBytes = (std::uint8_t*)(moduleHandle);

                std::size_t patternBytesSize = patternBytes.size();
                int* data = patternBytes.data();

                for (std::size_t i = 0; i < sizeOfImage - patternBytesSize; ++i) {
                    bool found = true;

                    for (std::size_t j = 0; j < patternBytesSize; ++j) {
                        if ((scanBytes[i + j] != data[j]) && (data[j] != -1)) {
                            found = false;
                            break;
                        }
                    }

                    if (found) {
                        return (std::uintptr_t)(&scanBytes[i]);
                    }
                }

                if (reportError) {
                    std::string errorMessage = std::format("Unknown signature in module {}: {}", moduleDescription, signature);
                    MessageBoxA(nullptr, errorMessage.c_str(), "Error", MB_ICONERROR);
                    throw std::runtime_error(errorMessage);
                }
                return 0;
            }
        }

        HMODULE GetGameModuleHandle() {
            HMODULE moduleHandle = GetModuleHandleA(kOriginalGameModuleName);
            if (moduleHandle == nullptr) {
                moduleHandle = GetModuleHandleA(kWrappedGameModuleName);
            }

            return moduleHandle;
        }

        std::uintptr_t FindPattern(HMODULE moduleHandle, const char* signature, bool reportError) {
            return FindPatternInModule(moduleHandle, signature, reportError);
        }

        void PatchMemory(void* destination, const void* source, std::size_t size) {
            DWORD oldProtect;
            VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE, &oldProtect);
            memcpy(destination, source, size);
            VirtualProtect(destination, size, oldProtect, &oldProtect);
        }
    }

    namespace Hooks {
        bool CreateHook(void* targetFunction, void* detourFunction, void** originalFunction) {
            return HookEngine::CreateHook(targetFunction, detourFunction, originalFunction);
        }

		bool EnableHook(void* targetFunction) {
			return HookEngine::EnableHook(targetFunction);
		}

        bool EnableAllHooks() {
			return HookEngine::EnableAllHooks();
        }

		bool DisableHook(void* targetFunction) {
			return HookEngine::DisableHook(targetFunction);
		}

        bool DisableAllHooks() {
            return HookEngine::DisableAllHooks();
        }
    }
}
