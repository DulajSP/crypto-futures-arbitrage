#include "common/ConfigManager.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

// Static member definitions (default values)
std::vector<std::string> ConfigManager::symbols_;
std::string ConfigManager::mode_ = "paper";
double ConfigManager::feesPercent_ = 0.04;
double ConfigManager::maxPosUsd_ = 1000.0;
double ConfigManager::minSpreadPercent_ = 0.05;
double ConfigManager::rebalanceMinSpread_ = 0.02;
double ConfigManager::checkIntervalSeconds_ = 1;

// Load configuration from JSON file.
void ConfigManager::load(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + filePath);
    }

    nlohmann::json config;
    file >> config;

    symbols_.clear();
    if (config.contains("symbols")) {
        for (const auto& sym : config["symbols"]) {
            symbols_.emplace_back(sym.get<std::string>());
        }
    }

    if (config.contains("mode")) {
        mode_ = config["mode"].get<std::string>();
    }

    if (config.contains("fees")) {
        feesPercent_ = config["fees"].get<double>();
    }

    if (config.contains("maxPosUsd")) {
        maxPosUsd_ = config["maxPosUsd"].get<double>();
    }

    if (config.contains("minSpreadPercent")) {
        minSpreadPercent_ = config["minSpreadPercent"].get<double>();
    }

    if (config.contains("rebalanceMinSpread")) {
        rebalanceMinSpread_ = config["rebalanceMinSpread"].get<double>();
    }

    if (config.contains("checkIntervalSec")) {
        checkIntervalSeconds_ = config["checkIntervalSec"].get<double>();
    }
}

// Getters for configuration parameters.
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