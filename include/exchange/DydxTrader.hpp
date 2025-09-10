#pragma once

#include "core/ITradeExecutor.hpp"
#include "common/Logger.hpp"


#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <atomic>
#include <chrono>

/// Live trade executor for dYdX v4 Futures
/// Endpoint: REST Broadcast via gRPC-Gateway
class DydxTrader : public ITradeExecutor {
public:
    DydxTrader(std::string mnemonic, double subaccountNumber);

    // Execute a trade (market order) for a symbol.
    Fill executeTrade(const std::string& symbol,
                      const std::string& side,
                      double price,
                      double maxQty) override;

    const std::string& exchange() const { return exchange_; }

    // Get the estimated block height based on the last known block and average block time.
    uint64_t getEstimatedBlockHeight() const;

private:
    std::string exchange_ = "dYdX Futures";
    std::string baseURL_ = "https://dydx-dao-api.polkachu.com:443";
    std::string mnemonic_;
    double subaccountNumber_;

    std::vector<uint8_t> privateKey_;
    std::vector<uint8_t> publicKey_;
    std::string address_;
    double accountNumber_ = 0;

    // Block tracking
    std::atomic<uint64_t> lastBlockHeight_{0};
    std::atomic<std::chrono::steady_clock::time_point> lastBlockTime_;
    const double averageBlockTimeSeconds_ = 0.9; 

    struct SymbolRules {
        uint32_t clobPairId;
        uint64_t stepBaseQuantums;
        int atomicResolution;
        uint32_t subticksPerTick;
        double stepSize;
    };

    // Rules cache
    mutable std::mutex rulesMtx_;
    std::unordered_map<std::string, SymbolRules> rulesCache_;

    // Exchange info to rules
    bool loadSymbolRulesIfNeeded(const std::string& symbol);
    bool parseRulesFromExchangeInfo(const std::string& symbol, const std::string& body);

    // Derive private and public keys from the mnemonic.
    void deriveKeys();

    // Fetch account information from the exchange.
    void getAccountInfo();

    // Sign a message using the private key.
    std::string signMessage(const std::string& signDocBytes);

    // Manage block height updates
    void startBlockTrackingThread();
    void updateBlockHeightLoop();
    uint64_t fetchCurrentBlockHeightREST();

    // Clamp and format quantums.
    bool clampQtyForSymbol(const std::string& symbol, double reqQty, double& clampedQty, uint64_t& quantums);

};
