#pragma once

#include <string>
#include <vector>

// Manages loading and accessing configuration parameters.
class ConfigManager {
public:
    // Loads configuration (default: "config.json").
    static void load(const std::string& filePath = "config.json");

    // Getters for configuration parameters.
    static std::vector<std::string> getSymbols();           // Returns trading symbols.
    static std::string getMode();                           // Returns mode (e.g., "paper", "live").
    static double getFeesPercent();                         // Returns paper trading fee percent.
    static double getMaxPosUsd();                           // Returns max USD position size per symbol.
    static double getMinSpreadPercent();                    // Returns minimum spread percent for arbitrage.
    static double getRebalanceMinSpread();                  // Returns minimum spread for rebalancing.
    static double getCheckIntervalSeconds();                // Returns interval for checking arbitrage.

private:
    // Cached configuration values.
    static std::vector<std::string> symbols_;
    static std::string mode_;
    static double feesPercent_;
    static double maxPosUsd_;
    static double minSpreadPercent_;
    static double rebalanceMinSpread_;
    static double checkIntervalSeconds_;
};