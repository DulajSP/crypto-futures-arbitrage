#pragma once

#include "core/PaperTrader.hpp"
#include "exchange/IExchangeClient.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Core engine for managing arbitrage logic, positions, and trade execution across multiple exchanges.
class ArbitrageEngine {
public:
    void addExchangeClient(const std::shared_ptr<IExchangeClient>& client);

    // Registers a trade executor for a specific exchange.
    void addExecutor(const std::string& exchangeName, const std::shared_ptr<ITradeExecutor>& exec);

    void setSymbols(const std::vector<std::string>& symbols);

    // Sets engine parameters: min spread %, check interval (sec), max USD position, rebalance min spread %.
    void setConfig(double minSpreadPercent, double checkIntervalSec, double maxPosUsd, double rebalanceMinSpread);

    void start();

private:
    struct ExchangePos {
        double usd = 0.0;
    };

    void checkArbitrage(const std::string& symbol);

    // Returns remaining USD room for a position, given side ("buy"/"sell").
    double remainingUsdRoom(const std::string& exchangeName, const std::string& symbol, const std::string& side);

    // Updates and returns new USD position after a trade.
    double applyPositionUpdate(const std::string& exchangeName, const std::string& symbol,
                             const std::string& side, double executedUsd);

    // Generates a unique key for (exchange, symbol) pairs.
    std::string posKey(const std::string& exchangeName, const std::string& symbol) const { return exchangeName + ":" + symbol; }

    std::vector<std::string> symbols_;
    std::vector<std::shared_ptr<IExchangeClient>> exchanges_;
    std::unordered_map<std::string, std::shared_ptr<ITradeExecutor>> executors_;
    std::unordered_map<std::string, ExchangePos> activePositionsUsd_;
    std::unordered_map<std::string, double> cumulativePnl_;

    // Engine configuration parameters
    double minSpreadPercent_ = 0.05;
    double checkIntervalSec_ = 1.0;
    double maxPosUsd_ = 10000;
    double rebalanceMinSpread_ = 0.01;
};