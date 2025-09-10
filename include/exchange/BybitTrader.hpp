#pragma once

#include "core/ITradeExecutor.hpp"
#include "common/Logger.hpp"

#include <string>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>


/// Live trade executor for Bybit USDT Perpetuals (v5).
/// Requires:
///  - apiKey:    API key string
///  - apiSecret: API secret string
class BybitTrader : public ITradeExecutor {
public:
    BybitTrader(std::string apiKey, std::string apiSecret);

    // Execute a trade (market order) for a symbol.
    Fill executeTrade(const std::string& symbol,
                      const std::string& side,
                      double price,
                      double maxQty) override;

    // Returns the exchange name ("Bybit Futures").
    const std::string& exchange() const { return exchange_; }

private:
    std::string exchange_ = "Bybit Futures";
    std::string baseURL_ = "https://api.bybit.com";
    std::string apiKey_;
    std::string apiSecret_;

    struct SymbolRules {
        double qtyStep = 0.0;
        double minQty  = 0.0;
        int qtyDp      = 6;
    };

    // Rules cache
    mutable std::mutex rulesMtx_; // Protects rulesCache_
    std::unordered_map<std::string, SymbolRules> rulesCache_; // Symbol -> rules

    // HMAC SHA256 signing.
    static std::string hmacSha256Hex(const std::string& secret, const std::string& msg);

    // Load symbol rules if not cached.
    bool loadSymbolRulesIfNeeded(const std::string& symbol);

    // Parse symbol rules from instruments info.
    bool parseRulesFromInstrumentsInfo(const std::string& symbol, const std::string& body);

    // Clamp and format quantity for symbol.
    bool clampQtyForSymbol(const std::string& symbol, double reqQty, double& clampedQty, std::string& qtyStr);
};