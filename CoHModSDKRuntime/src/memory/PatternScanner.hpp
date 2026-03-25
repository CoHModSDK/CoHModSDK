#pragma once

#include <cstdint>
#include <optional>

class Logger;

namespace PatternScanner {
    std::optional<std::uintptr_t> Find(const char* moduleName, const char* signature, Logger& logger);
}
