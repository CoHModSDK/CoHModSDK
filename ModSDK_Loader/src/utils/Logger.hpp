#pragma once

#include <filesystem>
#include <fstream>
#include <string>

class Logger {
public:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void Open(const std::filesystem::path& logPath);

    void LogInfo(const std::string& message);
    void LogWarning(const std::string& message);
    void LogError(const std::string& message);

    bool IsOpen() const;

private:
    std::string GetCurrentTimestamp() const;
    void LogMessage(const char* level, const std::string& message);

private:
    std::ofstream logFile;
};
