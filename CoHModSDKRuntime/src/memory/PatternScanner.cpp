#include "PatternScanner.hpp"

#include <Windows.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
    std::vector<int> PatternToBytes(const char* pattern) {
        std::vector<int> bytes;
        const char* current = pattern;
        const char* end = pattern + std::strlen(pattern);

        while (current < end) {
            if (*current == '?') {
                ++current;
                if ((current < end) && (*current == '?')) {
                    ++current;
                }

                bytes.push_back(-1);
            } else if (*current == ' ') {
                ++current;
            } else {
                char* next = nullptr;
                bytes.push_back(static_cast<int>(std::strtoul(current, &next, 16)));
                current = next;
            }
        }

        return bytes;
    }
}

namespace PatternScanner {
    std::optional<std::uintptr_t> Find(const char* moduleName, const char* signature) {
        if ((moduleName == nullptr) || (signature == nullptr)) {
            return std::nullopt;
        }

        HMODULE moduleHandle = GetModuleHandleA(moduleName);
        if (moduleHandle == nullptr) {
            return std::nullopt;
        }

        PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(moduleHandle);
        PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<std::uint8_t*>(moduleHandle) + dosHeader->e_lfanew);
        const std::size_t sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
        std::vector<int> patternBytes = PatternToBytes(signature);
        std::uint8_t* scanBytes = reinterpret_cast<std::uint8_t*>(moduleHandle);

        const std::size_t patternSize = patternBytes.size();
        if ((patternSize == 0u) || (sizeOfImage < patternSize)) {
            return std::nullopt;
        }

        for (std::size_t index = 0; index <= sizeOfImage - patternSize; ++index) {
            bool found = true;
            for (std::size_t patternIndex = 0; patternIndex < patternSize; ++patternIndex) {
                const int expectedByte = patternBytes[patternIndex];
                if ((expectedByte != -1) && (scanBytes[index + patternIndex] != expectedByte)) {
                    found = false;
                    break;
                }
            }

            if (found) {
                return reinterpret_cast<std::uintptr_t>(&scanBytes[index]);
            }
        }

        return std::nullopt;
    }

    bool MatchesBuffer(const std::uint8_t* buffer, std::size_t size, const char* signature) {
        if ((buffer == nullptr) || (signature == nullptr)) {
            return false;
        }

        const std::vector<int> patternBytes = PatternToBytes(signature);
        const std::size_t patternSize = patternBytes.size();
        if ((patternSize == 0) || (size < patternSize)) {
            return false;
        }

        for (std::size_t i = 0; i < patternSize; ++i) {
            if ((patternBytes[i] != -1) && (buffer[i] != static_cast<std::uint8_t>(patternBytes[i]))) {
                return false;
            }
        }

        return true;
    }
}
