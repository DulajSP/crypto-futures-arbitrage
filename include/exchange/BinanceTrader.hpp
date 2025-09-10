#pragma once

#include "core/ITradeExecutor.hpp"
#include "common/Logger.hpp"

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>

/// Live trade executor for Binance USDT-M Futures (REST-based).
/// Requires:
///  - apiKey:    API key string
///  - apiSecret: API secret string
class BinanceTrader : public ITradeExecutor {
public:
    BinanceTrader(std::string apiKey, std::string apiSecret);

    // Execute a trade (market order) for a symbol.
    Fill executeTrade(const std::string& symbol,
                      const std::string& side,
                      double price,
                      double maxQty) override;

    const std::string& exchange() const { return exchange_; }

private:
    std::string exchange_ = "Binance Futures";
    std::string baseURL_ = "https://fapi.binance.com";
    std::string apiKey_;
    std::string apiSecret_;

    struct SymbolRules {
        double qtyStep = 0.0;
        double minQty  = 0.0;
        double maxQty  = 0.0; 
        int qtyDp      = 6;   
    };

    // Rules cache
    mutable std::mutex rulesMtx_;
    std::unordered_map<std::string, SymbolRules> rulesCache_;

    // Signing 
    std::string signQuery(const std::string& qs) const;

    // Exchange info to rules
    bool loadSymbolRulesIfNeeded(const std::string& symbol);
    bool parseRulesFromExchangeInfo(const std::string& symbol, const std::string& body);

    // Clamp/format quantity
    bool clampQtyForSymbol(const std::string& symbol, double reqQty, double& clampedQty, std::string& qtyStr);
};