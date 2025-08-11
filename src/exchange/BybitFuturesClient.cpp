#include "exchange/BybitFuturesClient.hpp"
#include "common/Logger.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <thread>
#include <chrono>

BybitFuturesClient::BybitFuturesClient() {}

BybitFuturesClient::~BybitFuturesClient() {
    disconnect();
}

void BybitFuturesClient::connect() {
    if (connected_) {
        Logger::info("BybitFuturesClient is already connected.");
        return;
    }

    Logger::info("Connecting to Bybit Futures WebSocket...");
    connected_ = true;
}

void BybitFuturesClient::disconnect() {
    if (!connected_) return;

    Logger::info("Disconnecting from Bybit Futures...");

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [symbol, ws] : wsClients_) {
        ws->stop();
    }
    wsClients_.clear();
    connected_ = false;
}

void BybitFuturesClient::subscribeOrderBook(const std::string& symbol) {
    if (!connected_) {
        Logger::error("Cannot subscribe: BybitFuturesClient is not connected.");
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

void BybitFuturesClient::startWebSocket(const std::string& symbol) {
    std::string upperSymbol = symbol;
    std::transform(upperSymbol.begin(), upperSymbol.end(), upperSymbol.begin(), ::toupper);
    std::string topic = "orderbook.50." + upperSymbol;
    std::string url = "wss://stream.bybit.com/v5/public/linear";

    Logger::info("Connecting to Bybit Futures WebSocket for: " + symbol);

    std::shared_ptr<OrderBook> ob;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ob = orderBooks_[symbol];
    }

    auto ws = std::make_unique<ix::WebSocket>();
    ws->setUrl(url);

    ws->setOnMessageCallback([this, symbol, topic, ob](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto json = nlohmann::json::parse(msg->str);

                if (!json.contains("topic") || json["topic"] != topic) return;

                std::string type = json.value("type", "");
                const auto& data = json["data"];

                if (type == "snapshot") {
                    ob->clear(); // Full reset on snapshot

                    for (const auto& bid : data["b"]) {
                        double price = std::stod(bid[0].get<std::string>());
                        double qty   = std::stod(bid[1].get<std::string>());
                        ob->updateBid(price, qty);
                    }

                    for (const auto& ask : data["a"]) {
                        double price = std::stod(ask[0].get<std::string>());
                        double qty   = std::stod(ask[1].get<std::string>());
                        ob->updateAsk(price, qty);
                    }
                } else if (type == "delta") {
                    for (const auto& bid : data["b"]) {
                        double price = std::stod(bid[0].get<std::string>());
                        double qty   = std::stod(bid[1].get<std::string>());
                        ob->updateBid(price, qty);
                    }

                    for (const auto& ask : data["a"]) {
                        double price = std::stod(ask[0].get<std::string>());
                        double qty   = std::stod(ask[1].get<std::string>());
                        ob->updateAsk(price, qty);
                    }
                }
            } catch (const std::exception& ex) {
                Logger::error("Bybit WebSocket parse error: " + std::string(ex.what()));
            }
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            Logger::info("WebSocket opened for: " + symbol);

            nlohmann::json subscribeMsg = {
                {"op", "subscribe"},
                {"args", {topic}}
            };

            std::lock_guard<std::mutex> lock(mutex_);
            wsClients_[symbol]->send(subscribeMsg.dump());
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

void BybitFuturesClient::reconnectWithDelay(const std::string& symbol) {
    // Reconnect logic runs in a detached thread to avoid blocking
    std::thread([this, symbol]() {
        Logger::info("Reconnecting to Bybit for " + symbol + " after 3 seconds...");
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

std::shared_ptr<OrderBook> BybitFuturesClient::getOrderBook(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = orderBooks_.find(symbol);
    if (it != orderBooks_.end()) {
        return it->second;
    }
    return nullptr;
}

std::string BybitFuturesClient::getExchangeName() const {
    return "Bybit Futures";
}