#include "common/Logger.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <sstream>

namespace {
    // Returns current timestamp as a formatted string.
    std::string timestamp() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
}

void Logger::info(const std::string& msg) {
    std::cout << "[" << timestamp() << "] [INFO] " << msg << std::endl;
}

void Logger::warn(const std::string& msg) {
    std::cout << "[" << timestamp() << "] [WARN] " << msg << std::endl;
}

void Logger::error(const std::string& msg) {
    std::cerr << "[" << timestamp() << "] [ERROR] " << msg << std::endl;
}