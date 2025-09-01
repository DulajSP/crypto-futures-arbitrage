#include "exchange/BybitFuturesClient.hpp"
#include "common/Logger.hpp"
#include "core/Watchdog.hpp"

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
    orderBooks_.clear();
    reconnecting_.clear();
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

void BybitFuturesClient::requestReconnect(const std::string& symbol) {
    reconnectWithDelay(symbol);
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

    auto ws = std::make_shared<ix::WebSocket>();
    ws->setUrl(url);

    // weak_ptr so the lambda doesn't keep the socket alive
    std::weak_ptr<ix::WebSocket> wsWeak = ws;

    ws->setOnMessageCallback([this, symbol, topic, ob, wsWeak](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            Logger::info("Bybit WebSocket opened for: " + symbol);

            nlohmann::json subscribeMsg = {
                {"op", "subscribe"},
                {"args", {topic}}
            };

            if (auto s = wsWeak.lock()) {
                s->send(subscribeMsg.dump());
            }
            return;
        }

        if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto json = nlohmann::json::parse(msg->str);

                if (!json.contains("topic") || json["topic"] != topic) return;

                const std::string type = json.value("type", "");
                if (!json.contains("data")) return;
                const auto& data = json["data"];

                if (type == "snapshot") {
                    ob->clear(); // full reset
                    if (data.contains("b")) {
                        for (const auto& bid : data["b"]) {
                            double price = std::stod(bid[0].get<std::string>());
                            double qty   = std::stod(bid[1].get<std::string>());
                            ob->updateBid(price, qty);
                        }
                    }
                    if (data.contains("a")) {
                        for (const auto& ask : data["a"]) {
                            double price = std::stod(ask[0].get<std::string>());
                            double qty   = std::stod(ask[1].get<std::string>());
                            ob->updateAsk(price, qty);
                        }
                    }

                    // Notify watchdog of fresh orderbook update
                    if (watchdog_) watchdog_->markUpdate(getExchangeName(), "orderbook", symbol);

                } else if (type == "delta") {
                    if (data.contains("b")) {
                        for (const auto& bid : data["b"]) {
                            double price = std::stod(bid[0].get<std::string>());
                            double qty   = std::stod(bid[1].get<std::string>());
                            ob->updateBid(price, qty); 
                        }
                    }
                    if (data.contains("a")) {
                        for (const auto& ask : data["a"]) {
                            double price = std::stod(ask[0].get<std::string>());
                            double qty   = std::stod(ask[1].get<std::string>());
                            ob->updateAsk(price, qty);
                        }
                    }

                    // Notify watchdog of fresh orderbook update
                    if (watchdog_) watchdog_->markUpdate(getExchangeName(), "orderbook", symbol);

                }
            } catch (const std::exception& ex) {
                Logger::error("Bybit WebSocket parse error (" + symbol + "): " + std::string(ex.what()));
            }
            return;
        }

        if (msg->type == ix::WebSocketMessageType::Error) {
            Logger::error("Bybit WebSocket error for " + symbol + ": " + msg->errorInfo.reason);
            reconnectWithDelay(symbol);
            return;
        }

        if (msg->type == ix::WebSocketMessageType::Close) {
            Logger::info("Bybit WebSocket closed for symbol: " + symbol);
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

void BybitFuturesClient::reconnectWithDelay(const std::string& symbol) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (reconnecting_[symbol]) return;
        reconnecting_[symbol] = true;
    }

    std::thread([this, symbol]() {
        try {
            Logger::info("Reconnecting to Bybit for " + symbol + "...");
            std::this_thread::sleep_for(std::chrono::seconds(3));

            std::shared_ptr<ix::WebSocket> old;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = wsClients_.find(symbol);
                if (it != wsClients_.end()) {
                    old = it->second;
                    wsClients_.erase(it);
                    Logger::info("Stopping old Bybit WebSocket before reconnecting: " + symbol);
                }
            }
            if (old) old->stop();

            startWebSocket(symbol);

        } catch (const std::exception& e) {
            Logger::error("Reconnect thread exception for " + symbol + " (Bybit): " + e.what());
        } catch (...) {
            Logger::error("Reconnect thread unknown exception for " + symbol + " (Bybit)");
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            reconnecting_[symbol] = false;
        }
    }).detach();
}

std::shared_ptr<OrderBook> BybitFuturesClient::getOrderBook(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = orderBooks_.find(symbol);
    if (it != orderBooks_.end()) return it->second;
    return nullptr;
}

std::string BybitFuturesClient::getExchangeName() const {
    return "Bybit Futures";
}