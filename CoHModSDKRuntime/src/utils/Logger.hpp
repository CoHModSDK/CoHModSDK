#pragma once

#include <filesystem>
#include <fstream>
#include <string>

class Logger {
public:
    explicit Logger(const char* name = nullptr);
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void Open(const std::filesystem::path& logPath);
    void LogDebug(const std::string& message, const char* source = nullptr);
    void LogInfo(const std::string& message, const char* source = nullptr);
    void LogWarning(const std::string& message, const char* source = nullptr);
    void LogError(const std::string& message, const char* source = nullptr);
    bool IsOpen() const;

private:
    std::string GetCurrentTimestamp() const;
    void LogMessage(const char* level, const std::string& message, const char* source);

    const char* name = nullptr;
    std::ofstream logFile;
};
