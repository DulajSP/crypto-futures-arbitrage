#pragma once

#include "exchange/IExchangeClient.hpp"
#include "core/OrderBook.hpp"

#include <ixwebsocket/IXWebSocket.h>
#include <unordered_map>
#include <string>
#include <mutex>
#include <memory>

// Bybit USDT futures exchange client (WebSocket-based).
class BybitFuturesClient : public IExchangeClient {
public:
    BybitFuturesClient();
    ~BybitFuturesClient() override;

    // Establish WebSocket connection(s) to Bybit.
    void connect() override;

    // Disconnect all WebSocket connections.
    void disconnect() override;

    // Subscribe to order book updates for a symbol.
    void subscribeOrderBook(const std::string& symbol) override;

    // Get the current order book for a symbol.
    std::shared_ptr<OrderBook> getOrderBook(const std::string& symbol) const override;

    // Returns the exchange name ("bybit_futures").
    std::string getExchangeName() const override;

private:
    // Start a WebSocket connection for a symbol.
    void startWebSocket(const std::string& symbol);

    // Attempt to reconnect after a delay.
    void reconnectWithDelay(const std::string& symbol);

    mutable std::mutex mutex_; // Protects access to orderBooks_ and wsClients_
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> orderBooks_; // Symbol -> OrderBook
    std::unordered_map<std::string, std::unique_ptr<ix::WebSocket>> wsClients_; // Symbol -> WebSocket client
    bool connected_ = false; // Connection status
};