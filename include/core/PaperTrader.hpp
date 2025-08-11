#pragma once

#include "core/ITradeExecutor.hpp"
#include <string>

// Simulates trade execution for paper trading.
class PaperTrader : public ITradeExecutor {
public:
    // exchangeName must match IExchangeClient::getExchangeName() for mapping.
    PaperTrader(std::string exchangeName, double feePercent);

    // Simulate trade execution and return fill report.
    Fill executeTrade(
        const std::string& symbol,
        const std::string& side,
        double price,
        double maxQty
    ) override;

    // Returns the exchange name associated with this trader.
    const std::string& exchange() const { return exchange_; }

private:
    std::string exchange_; // Exchange identifier
    double feePct_;        // Fee percent (e.g. 0.04 = 0.04%)
};