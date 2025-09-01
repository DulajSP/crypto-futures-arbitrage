#include "exchange/BinanceFuturesClient.hpp"
#include "common/Logger.hpp"
#include "core/Watchdog.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <thread>
#include <chrono>

BinanceFuturesClient::BinanceFuturesClient() {}

BinanceFuturesClient::~BinanceFuturesClient() {
    disconnect();
}

void BinanceFuturesClient::connect() {
    if (connected_) {
        Logger::info("BinanceFuturesClient is already connected.");
        return;
    }

    Logger::info("Connecting to Binance Futures WebSocket...");
    connected_ = true;
}

void BinanceFuturesClient::disconnect() {
    if (!connected_) return;

    Logger::info("Disconnecting from Binance Futures...");

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [symbol, ws] : wsClients_) {
        ws->stop();
    }
    wsClients_.clear();
    orderBooks_.clear();
    reconnecting_.clear();
    connected_ = false;
}

void BinanceFuturesClient::subscribeOrderBook(const std::string& symbol) {
    if (!connected_) {
        Logger::error("Cannot subscribe: BinanceFuturesClient is not connected.");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (orderBooks_.find(symbol) == orderBooks_.end()) {
            orderBooks_[symbol] = std::make_shared<OrderBook>();
        }
    }

    startWebSocket(symbol);
}

void BinanceFuturesClient::requestReconnect(const std::string& symbol) {
    reconnectWithDelay(symbol);  
}

void BinanceFuturesClient::startWebSocket(const std::string& symbol) {
    std::string lowerSymbol = symbol;
    std::transform(lowerSymbol.begin(), lowerSymbol.end(), lowerSymbol.begin(), ::tolower);
    std::string url = "wss://fstream.binance.com/ws/" + lowerSymbol + "@depth5@100ms";

    Logger::info("Connecting to Binance Futures WebSocket for: " + symbol);

    std::shared_ptr<OrderBook> ob;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ob = orderBooks_[symbol];
    }

    auto ws = std::make_shared<ix::WebSocket>();
    ws->setUrl(url);

    ws->setOnMessageCallback([this, symbol, ob](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            Logger::info("Binance WebSocket opened for symbol: " + symbol);
            return;
        }

        if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto json = nlohmann::json::parse(msg->str);

                // Binance depth5 payload has "b" (bids) and "a" (asks) arrays of [price, qty] strings
                if (json.contains("b") && json.contains("a")) {
                    ob->clear();  // Depth5 is a full snapshot, so reset each message

                    for (const auto& bid : json["b"]) {
                        double price = std::stod(bid[0].get<std::string>());
                        double qty   = std::stod(bid[1].get<std::string>());
                        ob->updateBid(price, qty);
                    }

                    for (const auto& ask : json["a"]) {
                        double price = std::stod(ask[0].get<std::string>());
                        double qty   = std::stod(ask[1].get<std::string>());
                        ob->updateAsk(price, qty);
                    }

                    // Notify watchdog of fresh orderbook update
                    if (watchdog_) watchdog_->markUpdate(getExchangeName(), "orderbook", symbol);
                }
            } catch (const std::exception& ex) {
                Logger::error("Binance WebSocket parse error (" + symbol + "): " + std::string(ex.what()));
            }
            return;
        }

        if (msg->type == ix::WebSocketMessageType::Error) {
            Logger::error("Binance WebSocket error for " + symbol + ": " + msg->errorInfo.reason);
            reconnectWithDelay(symbol);
            return;
        }

        if (msg->type == ix::WebSocketMessageType::Close) {
            Logger::info("Binance WebSocket closed for symbol: " + symbol);
            reconnectWithDelay(symbol);
            return;
        }
    });

    ws->start();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        wsClients_[symbol] = ws; // store shared_ptr
    }
}

void BinanceFuturesClient::reconnectWithDelay(const std::string& symbol) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // prevent multiple concurrent reconnect threads for the same symbol
        if (reconnecting_[symbol]) return;
        reconnecting_[symbol] = true;
    }

    std::thread([this, symbol]() {
        try {
            Logger::info("Reconnecting to Binance for " + symbol + "...");
            std::this_thread::sleep_for(std::chrono::seconds(3));

            std::shared_ptr<ix::WebSocket> old;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = wsClients_.find(symbol);
                if (it != wsClients_.end()) {
                    old = it->second;
                    wsClients_.erase(it);
                    Logger::info("Stopping old Binance WebSocket before reconnecting: " + symbol);
                }
            }
            if (old) old->stop();

            startWebSocket(symbol);

        } catch (const std::exception& e) {
            Logger::error("Reconnect thread exception for " + symbol + ": " + e.what());
        } catch (...) {
            Logger::error("Reconnect thread unknown exception for " + symbol);
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            reconnecting_[symbol] = false;
        }
    }).detach();
}

std::shared_ptr<OrderBook> BinanceFuturesClient::getOrderBook(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = orderBooks_.find(symbol);
    if (it != orderBooks_.end()) {
        return it->second;
    }
    return nullptr;
}

std::string BinanceFuturesClient::getExchangeName() const {
    return "Binance Futures";
}