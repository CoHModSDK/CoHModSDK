#include "Logger.hpp"

#include <Windows.h>

#include <cstdio>
#include <filesystem>
#include <system_error>

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

void Logger::LogDebug(const std::string& message, const char* source) {
    LogMessage("DEBUG", message, source);
}

void Logger::LogInfo(const std::string& message, const char* source) {
    LogMessage("INFO", message, source);
}

void Logger::LogWarning(const std::string& message, const char* source) {
    LogMessage("WARN", message, source);
}

void Logger::LogError(const std::string& message, const char* source) {
    LogMessage("ERROR", message, source);
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

void Logger::LogMessage(const char* level, const std::string& message, const char* source) {
    if (!logFile.is_open()) {
        return;
    }

    logFile << GetCurrentTimestamp();
    if ((source != nullptr) && (*source != '\0')) {
        logFile << "[" << source << "]";
    }

    logFile << "[" << level << "] " << message << std::endl;
    logFile.flush();
}
