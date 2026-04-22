#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace PatternScanner {
    std::optional<std::uintptr_t> Find(const char* moduleName, const char* signature);
    bool MatchesBuffer(const std::uint8_t* buffer, std::size_t size, const char* signature);
}
