#include "common/ConfigManager.hpp"
#include "common/Logger.hpp"
#include "core/ArbitrageEngine.hpp"
#include "exchange/BinanceFuturesClient.hpp"
#include "exchange/BybitFuturesClient.hpp"

int main() {
    Logger::info("=== Starting Arbitrage Bot ===");

    // Load configuration from file
    ConfigManager::load("config.json");

    std::string mode = ConfigManager::getMode();
    double fees = ConfigManager::getFeesPercent();
    double maxPos = ConfigManager::getMaxPosUsd();
    double minSpread = ConfigManager::getMinSpreadPercent();
    double rebalanceMinSpread = ConfigManager::getRebalanceMinSpread();
    double intervalSec = ConfigManager::getCheckIntervalSeconds();
    auto symbols = ConfigManager::getSymbols();

    // Set up exchange clients
    auto binance = std::make_shared<BinanceFuturesClient>();
    auto bybit = std::make_shared<BybitFuturesClient>();
    binance->connect();
    bybit->connect();

    // Subscribe to order books for all symbols
    for (const auto& sym : symbols) {
        binance->subscribeOrderBook(sym);
        bybit->subscribeOrderBook(sym);
    }

    // Set up arbitrage engine
    ArbitrageEngine engine;
    engine.addExchangeClient(binance);
    engine.addExchangeClient(bybit);
    engine.setSymbols(symbols);
    engine.setConfig(minSpread, intervalSec, maxPos, rebalanceMinSpread);
    
    // Register executors: paper or live
    if (mode == "paper") {
        // Exchange names must exactly match getExchangeName()
        engine.addExecutor(binance->getExchangeName(), std::make_shared<PaperTrader>(binance->getExchangeName(), fees));
        engine.addExecutor(bybit->getExchangeName(),   std::make_shared<PaperTrader>(bybit->getExchangeName(),   fees));
    } else {
        // TODO: add LiveTrader executors 
    }

    // Start main arbitrage loop
    engine.start();

    return 0;
}