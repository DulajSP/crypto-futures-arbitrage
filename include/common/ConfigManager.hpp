#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// Manages loading and accessing configuration parameters.
class ConfigManager {
public:
    // Loads configuration (default: "config.json").
    static void load(const std::string& filePath = "config.json");

    // Generic accessors
    static std::string getString(const std::string& path,
                                 const std::string& def = "");
    static double getNumber(const std::string& path,
                            double def = 0.0);

    // Convenience wrappers
    static std::vector<std::string> getSymbols();           // Returns trading symbols.
    static std::string getMode();                           // Returns mode (e.g., "paper", "live").
    static double getFeesPercent();                         // Returns paper trading fee percent.
    static double getMaxPosUsd();                           // Returns max USD position size per symbol.
    static double getMinSpreadPercent();                    // Returns minimum spread percent for arbitrage.
    static double getRebalanceMinSpread();                  // Returns minimum spread for rebalancing.
    static double getCheckIntervalSeconds();                // Returns interval for checking arbitrage.

private:
    static nlohmann::json config_; // full JSON cache

    // Cached common values
    static std::vector<std::string> symbols_;
    static std::string mode_;
    static double feesPercent_;
    static double maxPosUsd_;
    static double minSpreadPercent_;
    static double rebalanceMinSpread_;
    static double checkIntervalSeconds_;
};