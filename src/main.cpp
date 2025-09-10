#include "common/ConfigManager.hpp"
#include "common/Logger.hpp"
#include "core/ArbitrageEngine.hpp"
#include "core/Watchdog.hpp"

#include "exchange/BinanceFuturesClient.hpp"
#include "exchange/BybitFuturesClient.hpp"
#include "exchange/DydxFuturesClient.hpp"

#include "core/PaperTrader.hpp"
#include "exchange/BinanceTrader.hpp"
#include "exchange/BybitTrader.hpp"
#include "exchange/DydxTrader.hpp"

bool setupExchange(const std::shared_ptr<IExchangeClient>& client, ArbitrageEngine& engine, Watchdog& watchdog, const std::vector<std::string>& symbols, bool enabled) {
    if (!enabled) return false;
    client->connect();
    client->setWatchdog(&watchdog);
    watchdog.registerMarketClient(client, symbols);
    for (const auto& sym : symbols) client->subscribeOrderBook(sym);
    engine.addExchangeClient(client);
    return true;
}

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
    auto dydx = std::make_shared<DydxFuturesClient>();   

    // Set up arbitrage engine
    ArbitrageEngine engine;
    engine.setSymbols(symbols);
    engine.setConfig(minSpread, intervalSec, maxPos, rebalanceMinSpread);

    bool binanceOk = false, bybitOk = false, dydxOk = false;

     // Register executors: paper or live
    if (mode == "paper") {
        binanceOk = bybitOk = dydxOk = true;
        engine.addExecutor(binance->getExchangeName(), std::make_shared<PaperTrader>(binance->getExchangeName(), fees));
        engine.addExecutor(bybit->getExchangeName(),   std::make_shared<PaperTrader>(bybit->getExchangeName(),   fees));
        engine.addExecutor(dydx->getExchangeName(),    std::make_shared<PaperTrader>(dydx->getExchangeName(),    fees)); 
    } else if (mode == "live") {
        // Load API keys from config.json
        std::string binanceKey     = ConfigManager::getString("binance.api_key");
        std::string binanceSecret  = ConfigManager::getString("binance.secret");
        std::string bybitKey       = ConfigManager::getString("bybit.api_key");
        std::string bybitSecret    = ConfigManager::getString("bybit.secret");
        std::string dydxMnemonic   = ConfigManager::getString("dydx.mnemonic");
        double dydxSubaccount      = ConfigManager::getNumber("dydx.subaccount_number");

        if (!binanceKey.empty() && !binanceSecret.empty()) {
            binanceOk = true;
            engine.addExecutor(binance->getExchangeName(), 
                        std::make_shared<BinanceTrader>(binanceKey, binanceSecret));
        } else {
            Logger::warn("[LIVE] Skipping Binance executor: API key/secret missing.");
        }

        if (!bybitKey.empty() && !bybitSecret.empty()) {
            bybitOk = true;
            engine.addExecutor(bybit->getExchangeName(), 
                        std::make_shared<BybitTrader>(bybitKey, bybitSecret));
        } else {
            Logger::warn("[LIVE] Skipping Bybit executor: API key/secret missing.");
        }

        if (!dydxMnemonic.empty()) {
            dydxOk = true;
            engine.addExecutor(dydx->getExchangeName(), 
                        std::make_shared<DydxTrader>(dydxMnemonic, dydxSubaccount));
        } else {
            Logger::warn("[LIVE] Skipping dYdX executor: mnemonic missing.");
        }

        int executorCount = (int)binanceOk + (int)bybitOk + (int)dydxOk;
        if (executorCount < 2) {
            Logger::error("[LIVE] At least two valid executors are required to run the arbitrage engine.");
            return 1;
        }

    } else {
        Logger::error("Invalid mode in config.json: " + mode);
        return 1;
    }

    // --- Watchdog & exchange setup ---
    Watchdog watchdog;

    setupExchange(binance, engine, watchdog, symbols, binanceOk);
    setupExchange(bybit,   engine, watchdog, symbols, bybitOk);
    setupExchange(dydx,    engine, watchdog, symbols, dydxOk);

    watchdog.start();
    
    // Start main arbitrage loop
    engine.start();


    return 0;
}