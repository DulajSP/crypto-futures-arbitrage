#include "exchange/BinanceFuturesClient.hpp"
#include "common/Logger.hpp"

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
    connected_ = false;
}

void BinanceFuturesClient::subscribeOrderBook(const std::string& symbol) {
    if (!connected_) {
        Logger::error("Cannot subscribe: BinanceFuturesClient is not connected.");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Create order book if not already present
        if (orderBooks_.find(symbol) == orderBooks_.end()) {
            orderBooks_[symbol] = std::make_shared<OrderBook>();
        }
    }

    startWebSocket(symbol);
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

    auto ws = std::make_unique<ix::WebSocket>();
    ws->setUrl(url);

    ws->setOnMessageCallback([this, symbol, ob](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto json = nlohmann::json::parse(msg->str);
                if (json.contains("b") && json.contains("a")) {
                    ob->clear();  // Full reset

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
                }
            } catch (const std::exception& ex) {
                Logger::error("Binance WebSocket parse error: " + std::string(ex.what()));
            }
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            Logger::info("WebSocket opened for symbol: " + symbol);
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            Logger::error("WebSocket error for " + symbol + ": " + msg->errorInfo.reason);
            reconnectWithDelay(symbol);
        } else if (msg->type == ix::WebSocketMessageType::Close) {
            Logger::info("WebSocket closed for symbol: " + symbol);
            reconnectWithDelay(symbol);
        }
    });

    ws->start();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        wsClients_[symbol] = std::move(ws);
    }
}

void BinanceFuturesClient::reconnectWithDelay(const std::string& symbol) {
    // Reconnect logic runs in a detached thread to avoid blocking
    std::thread([this, symbol]() {
        Logger::info("Reconnecting to Binance for " + symbol + " after 3 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(3));

        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = wsClients_.find(symbol);
            if (it != wsClients_.end()) {
                Logger::info("Stopping old WebSocket before reconnecting: " + symbol);
                it->second->stop();
                wsClients_.erase(it);
            }
        }

        startWebSocket(symbol);
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