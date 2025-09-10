#include "common/ConfigManager.hpp"
#include <fstream>
#include <stdexcept>

using nlohmann::json;

// Static member definitions (default values)
json ConfigManager::config_;
std::vector<std::string> ConfigManager::symbols_;
std::string ConfigManager::mode_ = "paper";
double ConfigManager::feesPercent_ = 0.04;
double ConfigManager::maxPosUsd_ = 1000.0;
double ConfigManager::minSpreadPercent_ = 0.05;
double ConfigManager::rebalanceMinSpread_ = 0.02;
double ConfigManager::checkIntervalSeconds_ = 1.0;

// Load configuration from JSON file.
void ConfigManager::load(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + filePath);
    }

    file >> config_;

    symbols_.clear();
    if (config_.contains("symbols")) {
        for (const auto& sym : config_["symbols"]) {
            symbols_.emplace_back(sym.get<std::string>());
        }
    }

    if (config_.contains("mode")) {
        mode_ = config_["mode"].get<std::string>();
    }

    if (config_.contains("fees")) {
        feesPercent_ = config_["fees"].get<double>();
    }

    if (config_.contains("maxPosUsd")) {
        maxPosUsd_ = config_["maxPosUsd"].get<double>();
    }

    if (config_.contains("minSpreadPercent")) {
        minSpreadPercent_ = config_["minSpreadPercent"].get<double>();
    }

    if (config_.contains("rebalanceMinSpread")) {
        rebalanceMinSpread_ = config_["rebalanceMinSpread"].get<double>();
    }

    if (config_.contains("checkIntervalSec")) {
        checkIntervalSeconds_ = config_["checkIntervalSec"].get<double>();
    }
}

std::string ConfigManager::getString(const std::string& path, const std::string& def) {
    try {
        const json* cur = &config_;
        size_t start = 0, end;
        while ((end = path.find('.', start)) != std::string::npos) {
            std::string key = path.substr(start, end - start);
            if (!cur->contains(key)) return def;
            cur = &(*cur)[key];
            start = end + 1;
        }
        std::string key = path.substr(start);
        if (!cur->contains(key)) return def;
        return (*cur)[key].get<std::string>();
    } catch (...) {
        return def;
    }
}

double ConfigManager::getNumber(const std::string& path, double def) {
    try {
        const json* cur = &config_;
        size_t start = 0, end;
        while ((end = path.find('.', start)) != std::string::npos) {
            std::string key = path.substr(start, end - start);
            if (!cur->contains(key)) return def;
            cur = &(*cur)[key];
            start = end + 1;
        }
        std::string key = path.substr(start);
        if (!cur->contains(key)) return def;
        return (*cur)[key].get<double>();
    } catch (...) {
        return def;
    }
}

std::vector<std::string> ConfigManager::getSymbols() {
    return symbols_;
}

std::string ConfigManager::getMode() {
    return mode_;
}

double ConfigManager::getFeesPercent() {
    return feesPercent_;
}

double ConfigManager::getMaxPosUsd() {
    return maxPosUsd_;
}

double ConfigManager::getMinSpreadPercent() {
    return minSpreadPercent_;
}

double ConfigManager::getRebalanceMinSpread() {
    return rebalanceMinSpread_;
}

double ConfigManager::getCheckIntervalSeconds() {
    return checkIntervalSeconds_;
}