#include "exchange/DydxFuturesClient.hpp"
#include "common/Logger.hpp"
#include "core/Watchdog.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cctype>

DydxFuturesClient::DydxFuturesClient() {}

DydxFuturesClient::~DydxFuturesClient() {
    disconnect();
}

void DydxFuturesClient::connect() {
    if (connected_) {
        Logger::info("DydxFuturesClient is already connected.");
        return;
    }
    Logger::info("Connecting to dYdX v4 WebSocket...");
    connected_ = true;
}

void DydxFuturesClient::disconnect() {
    if (!connected_) return;

    Logger::info("Disconnecting from dYdX v4...");
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [sym, ws] : wsClients_) {
        ws->stop();
    }
    wsClients_.clear();
    orderBooks_.clear();
    reconnecting_.clear();
    connected_ = false;
}

void DydxFuturesClient::subscribeOrderBook(const std::string& engineSymbol) {
    if (!connected_) {
        Logger::error("Cannot subscribe: DydxFuturesClient is not connected.");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (orderBooks_.find(engineSymbol) == orderBooks_.end()) {
            orderBooks_[engineSymbol] = std::make_shared<OrderBook>();
        }
    }

    startWebSocket(engineSymbol);
}

void DydxFuturesClient::requestReconnect(const std::string& symbol) {
    reconnectWithDelay(symbol);
}

void DydxFuturesClient::startWebSocket(const std::string& engineSymbol) {
    // Engine symbol is like "BTCUSDT"; convert to dYdX "BTC-USD"
    const std::string dydxSym = toDydxSymbol(engineSymbol);
    const std::string url = "wss://indexer.dydx.trade/v4/ws";

    Logger::info("Connecting to dYdX v4 orderbook for: " + engineSymbol + " (as " + dydxSym + ")");

    std::shared_ptr<OrderBook> ob;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ob = orderBooks_[engineSymbol];
    }

    auto ws = std::make_shared<ix::WebSocket>();
    ws->setUrl(url);
    std::weak_ptr<ix::WebSocket> wsWeak = ws;

    ws->setOnMessageCallback([this, engineSymbol, dydxSym, ob, wsWeak](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            Logger::info("dYdX WebSocket opened for " + engineSymbol + " (" + dydxSym + ")");

            nlohmann::json sub = {
                {"type", "subscribe"},
                {"channel", "v4_orderbook"},
                {"id", dydxSym}
            };
            if (auto wsShared = wsWeak.lock()) {
                wsShared->send(sub.dump());
            }
            return;
        }

        if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto j = nlohmann::json::parse(msg->str);

                if (j.contains("type") && j["type"] == "subscribed") {
                    if (j.contains("contents")) {
                        auto& c = j["contents"];
                        bool hasSnap = (c.contains("bids") || c.contains("asks"));
                        if (hasSnap) {
                            ob->clear();
                            if (c.contains("bids")) {
                                for (const auto& lvl : c["bids"]) {
                                    double price = std::stod(lvl["price"].get<std::string>());
                                    double qty   = std::stod(lvl["size"].get<std::string>());
                                    ob->updateBid(price, qty);
                                }
                            }
                            if (c.contains("asks")) {
                                for (const auto& lvl : c["asks"]) {
                                    double price = std::stod(lvl["price"].get<std::string>());
                                    double qty   = std::stod(lvl["size"].get<std::string>());
                                    ob->updateAsk(price, qty);
                                }
                            }
                            Logger::info("dYdX snapshot loaded for " + engineSymbol);


                            // Notify watchdog of fresh orderbook update
                            if (watchdog_) watchdog_->markUpdate(getExchangeName(), "orderbook", engineSymbol);
                        }
                    }
                    return;
                }

                if (j.contains("type") && j["type"] == "channel_data") {
                    if (j.contains("contents")) {
                        auto& c = j["contents"];
                        if (c.contains("bids")) {
                            for (const auto& lvl : c["bids"]) {
                                double price = std::stod(lvl[0].get<std::string>());
                                double qty   = std::stod(lvl[1].get<std::string>());
                                ob->updateBid(price, qty);
                            }
                        }
                        if (c.contains("asks")) {
                            for (const auto& lvl : c["asks"]) {
                                double price = std::stod(lvl[0].get<std::string>());
                                double qty   = std::stod(lvl[1].get<std::string>());
                                ob->updateAsk(price, qty);
                            }
                        }

                        // Notify watchdog of fresh orderbook update
                        if (watchdog_) watchdog_->markUpdate(getExchangeName(), "orderbook", engineSymbol);
                    }
                    return;
                }

            } catch (const std::exception& ex) {
                Logger::error("dYdX parse error (" + engineSymbol + "): " + std::string(ex.what()));
            }
        } else if (msg->type == ix::WebSocketMessageType::Error || msg->type == ix::WebSocketMessageType::Close) {
            Logger::info("dYdX WebSocket closed or errored for " + engineSymbol);
            reconnectWithDelay(engineSymbol);
        }
    });

    ws->start();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        wsClients_[engineSymbol] = ws;
    }
}

void DydxFuturesClient::reconnectWithDelay(const std::string& engineSymbol) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (reconnecting_[engineSymbol]) return;
        reconnecting_[engineSymbol] = true;
    }

    std::thread([this, engineSymbol]() {
        try {
            Logger::info("Reconnecting dYdX for " + engineSymbol + "...");
            std::this_thread::sleep_for(std::chrono::seconds(3));

            std::shared_ptr<ix::WebSocket> old;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = wsClients_.find(engineSymbol);
                if (it != wsClients_.end()) {
                    old = it->second;
                    wsClients_.erase(it);
                    Logger::info("Stopping old dYdX WebSocket before reconnecting: " + engineSymbol);
                } 
            }
            if (old) old->stop();

            startWebSocket(engineSymbol);

        } catch (const std::exception& e) {
            Logger::error("Reconnect thread exception for " + engineSymbol + " (dYdX): " + std::string(e.what()));
        } catch (...) {
            Logger::error("Reconnect thread unknown exception for " + engineSymbol + " (dYdX)");
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            reconnecting_[engineSymbol] = false;
        }
    }).detach();
}

std::shared_ptr<OrderBook> DydxFuturesClient::getOrderBook(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = orderBooks_.find(symbol);
    if (it != orderBooks_.end()) return it->second;
    return nullptr;
}

std::string DydxFuturesClient::getExchangeName() const {
    return "dYdX Futures";
}

// Symbol conversions
// "BTCUSDT" (engine) -> "BTC-USD" (dYdX)
std::string DydxFuturesClient::toDydxSymbol(const std::string& engineSymbol) {
    std::string s = engineSymbol;
    // Basic rule: if ends with "USDT", replace with "-USD"
    const std::string suffix = "USDT";
    if (s.size() > suffix.size() && s.rfind(suffix) == s.size() - suffix.size()) {
        std::string base = s.substr(0, s.size() - suffix.size());
        return base + "-USD";
    }
    // If already looks like "BTC-USD", return as-is
    if (s.find('-') != std::string::npos) return s;

    return s; 
}

// "BTC-USD" (dYdX) -> "BTCUSDT" (engine)
std::string DydxFuturesClient::fromDydxSymbol(const std::string& dydxSymbol) {
    std::string s = dydxSymbol;
    auto pos = s.find('-');
    if (pos != std::string::npos) {
        std::string base = s.substr(0, pos);
        std::string quote = s.substr(pos + 1);
        if (quote == "USD") quote = "USDT"; // engine uses USDT-quoted symbols
        return base + quote;
    }
    return s;
}