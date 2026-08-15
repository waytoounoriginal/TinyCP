//
// Created for the tcp_from_scratch stack.
//

#include "utils/Logger.h"
#include "utils/Platform.h"

#include <ctime>
#include <iomanip>
#include <iostream>

const char* LevelName(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    return "?";
}

void Logger::Write(LogLevel level, const std::string& message) {
    if (level < level_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    char timestamp[32] = {};
    const std::time_t now = std::time(nullptr);
    std::tm time_info = {};
    localtime_r(&now, &time_info);
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
                  &time_info);

    std::cout << '[' << timestamp << "][" << LevelName(level) << "] "
              << message << '\n';
}