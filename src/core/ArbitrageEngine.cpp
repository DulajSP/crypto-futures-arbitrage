#include "core/ArbitrageEngine.hpp"
#include "common/Logger.hpp"
#include <thread>
#include <chrono>
#include <limits>

void ArbitrageEngine::addExchangeClient(const std::shared_ptr<IExchangeClient>& client) {
    exchanges_.push_back(client);
}

void ArbitrageEngine::addExecutor(const std::string& exchangeName,
                                  const std::shared_ptr<ITradeExecutor>& exec) {
    executors_[exchangeName] = exec;
}

void ArbitrageEngine::setSymbols(const std::vector<std::string>& symbols) {
    symbols_ = symbols;
}

void ArbitrageEngine::setConfig(double minSpreadPercent, double checkIntervalSec, double maxPosUsd, double rebalanceMinSpread) {
    minSpreadPercent_ = minSpreadPercent;
    checkIntervalSec_ = checkIntervalSec;
    maxPosUsd_ = maxPosUsd;
    rebalanceMinSpread_ = rebalanceMinSpread;
}

// Calculate remaining USD room for a position on a given exchange and symbol.
double ArbitrageEngine::remainingUsdRoom(const std::string& exchangeName,
                                         const std::string& symbol,
                                         const std::string& side) {
    const auto key = posKey(exchangeName, symbol);
    auto it = activePositionsUsd_.find(key);
    double cur = (it == activePositionsUsd_.end()) ? 0.0 : it->second.usd;

    if (side == "buy") {
        if (cur >= 0.0) return std::max(0.0, maxPosUsd_ - cur);
        return maxPosUsd_ - cur; // cur < 0 => room increases
    } else {
        if (cur <= 0.0) return std::max(0.0, maxPosUsd_ - std::fabs(cur));
        return maxPosUsd_ + cur; // cur > 0 => room increases
    }
}

// Update and return new USD position after a trade.
double ArbitrageEngine::applyPositionUpdate(const std::string& exchangeName, const std::string& symbol,
                                          const std::string& side, double executedUsd) {
    auto& pos = activePositionsUsd_[posKey(exchangeName, symbol)];
    if (side == "buy") pos.usd += executedUsd;
    else               pos.usd -= executedUsd;

    // Avoid floating point drift near zero.
    if (std::fabs(pos.usd) < 1e-6) pos.usd = 0.0;

    return pos.usd;
}

void ArbitrageEngine::start() {
    Logger::info("Starting Arbitrage Engine...");
    while (true) {
        for (const auto& symbol : symbols_) {
            checkArbitrage(symbol);
        }
        std::this_thread::sleep_for(std::chrono::duration<double>(checkIntervalSec_));
    }
}

// Core arbitrage opportunity detection and execution logic.
void ArbitrageEngine::checkArbitrage(const std::string& symbol) {
    double bestBid = 0.0, bestAsk = std::numeric_limits<double>::max();
    double bestBidQty = 0.0, bestAskQty = 0.0;

    std::shared_ptr<IExchangeClient> bidExchange = nullptr, askExchange = nullptr;

    for (const auto& exchange : exchanges_) {
        auto ob = exchange->getOrderBook(symbol);
        if (!ob) continue;

        if (ob->getTopBidPrice() > bestBid) {
            bestBid = ob->getTopBidPrice();
            bestBidQty = ob->getTopBidQty();
            bidExchange = exchange;
        }

        if (ob->getTopAskPrice() < bestAsk) {
            bestAsk = ob->getTopAskPrice();
            bestAskQty = ob->getTopAskQty();
            askExchange = exchange;
        }
    }

    if (bestBid <= 0.0 || bestAsk >= bestBid) return;

    double spreadPct = ((bestBid - bestAsk) / bestAsk) * 100.0;

    if (spreadPct > minSpreadPercent_) {

        std::string exchangeSell = bidExchange ? bidExchange->getExchangeName() : "Unknown";
        std::string exchangeBuy = askExchange ? askExchange->getExchangeName() : "Unknown";

        // check executors exist for both exchanges
        auto itBuyExec  = executors_.find(exchangeBuy);
        auto itSellExec = executors_.find(exchangeSell);
        if (itBuyExec == executors_.end() || itSellExec == executors_.end()) return;

        // Cap by orderbook quantities (base)
        double obCapQty = std::min(bestAskQty, bestBidQty);
        if (obCapQty <= 0.0) return;

        // Cap by per-venue same-side max USD
        double buyRoomUsd  = remainingUsdRoom(exchangeBuy,  symbol, "buy");
        double sellRoomUsd = remainingUsdRoom(exchangeSell, symbol, "sell");
        if (buyRoomUsd <= 0.0 || sellRoomUsd <= 0.0) return;

        double buyCapQty  = buyRoomUsd  / bestAsk;
        double sellCapQty = sellRoomUsd / bestBid;

        double reqQty = std::max(0.0, std::min({ obCapQty, buyCapQty, sellCapQty }));
        if (reqQty <= 0.0) return;

        Logger::info("ARB " + symbol + " | BUY " + exchangeBuy + " @" + std::to_string(bestAsk) +
                    " | SELL " + exchangeSell + " @" + std::to_string(bestBid) +
                    " | Spread=" + std::to_string(spreadPct) + "% | Qty=" + std::to_string(reqQty));

        // Execute both legs
        Fill buyFill  = itBuyExec->second->executeTrade(symbol, "buy",  bestAsk, reqQty);
        Fill sellFill = itSellExec->second->executeTrade(symbol, "sell", bestBid, reqQty);

        if (!buyFill.ok || !sellFill.ok) return;

        // Handle partials conservatively
        double execUSD = std::min(buyFill.cost, sellFill.cost);
        if (execUSD <= 0.0) return;

        // Pair PnL
        double gross = ((sellFill.price - buyFill.price) / buyFill.price ) * execUSD ;
        // reduce fees 
        double net  = gross - (buyFill.fee + sellFill.fee);

        cumulativePnl_[symbol] += net;

        // Update positions by venue
        double exchangeBuyPos = applyPositionUpdate(exchangeBuy,  symbol, "buy",  buyFill.cost);
        double exchangeSellPos = applyPositionUpdate(exchangeSell, symbol, "sell", sellFill.cost);

        Logger::info("EXEC " + symbol +
                    " | total=$" + std::to_string(execUSD) +
                    " | netPnL=$" + std::to_string(net) +
                    " | cumPnL=$" + std::to_string(cumulativePnl_[symbol])+
                    " | " + exchangeBuy + " pos=$" + std::to_string(exchangeBuyPos)+
                    " | " + exchangeSell + " pos=$" + std::to_string(exchangeSellPos)+ "\n");
        
    }
    else if (spreadPct > rebalanceMinSpread_) {
        // TODO: Rebalance logic if spread is above rebalanceMinSpread
    }
}