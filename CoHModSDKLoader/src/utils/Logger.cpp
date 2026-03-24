#include <Windows.h>
#include <system_error>
#include <filesystem>
#include <fstream>
#include "Logger.hpp"
#include <cstdio>

void Logger::Open(const std::filesystem::path& logPath) {
    if (logFile.is_open()) {
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(logPath.parent_path(), error);
    if (error) {
        return;
    }

    const std::filesystem::path backupLogPath = logPath.string() + ".bak";
    if (std::filesystem::exists(backupLogPath)) {
        std::filesystem::remove(backupLogPath, error);
        if (error) {
            return;
        }
    }

    if (std::filesystem::exists(logPath)) {
        std::filesystem::rename(logPath, backupLogPath, error);
        if (error) {
            return;
        }
    }

    logFile.open(logPath, std::ios::out | std::ios::trunc);
    if (logFile.is_open()) {
        LogDebug("Log file created");
    }
}

bool Logger::IsOpen() const {
    return logFile.is_open();
}

std::string Logger::GetCurrentTimestamp() const {
    SYSTEMTIME st = {};
    GetLocalTime(&st);

    char buffer[32] = {};
    sprintf_s(
        buffer,
        "[%02d:%02d:%02d]",
        st.wHour,
        st.wMinute,
        st.wSecond
    );

    return buffer;
}

void Logger::LogMessage(const char* level, const std::string& message) {
    if (!logFile.is_open()) {
        return;
    }

    logFile << GetCurrentTimestamp() << "[" << level << "] " << message << std::endl;
    logFile.flush();
}

void Logger::LogDebug(const std::string& message) {
    LogMessage("DEBUG", message);
}

void Logger::LogInfo(const std::string& message) {
    LogMessage("INFO", message);
}

void Logger::LogWarning(const std::string& message) {
    LogMessage("WARN", message);
}

void Logger::LogError(const std::string& message) {
    LogMessage("ERROR", message);
}
