#pragma once

#include "exchange/IExchangeClient.hpp"
#include "core/OrderBook.hpp"

#include <ixwebsocket/IXWebSocket.h>
#include <unordered_map>
#include <string>
#include <mutex>
#include <memory>

// dYdX v4 futures exchange client (WebSocket-based).
class DydxFuturesClient : public IExchangeClient {
public:
    DydxFuturesClient();
    ~DydxFuturesClient() override;

    // Establish WebSocket connection(s) to Dydx.
    void connect() override;

    // Disconnect all WebSocket connections.
    void disconnect() override;

    // Subscribe to order book updates for a symbol.
    void subscribeOrderBook(const std::string& symbol) override;

    // Get the current order book for a symbol.
    std::shared_ptr<OrderBook> getOrderBook(const std::string& symbol) const override;

    // Returns the exchange name.
    std::string getExchangeName() const override;

private:
    // Start a WebSocket connection for a symbol.
    void startWebSocket(const std::string& symbol);

    // Attempt to reconnect after a delay.
    void reconnectWithDelay(const std::string& symbol);

    // Symbol conversion helpers:
    // Engine format (e.g. "BTCUSDT") <-> dYdX format (e.g. "BTC-USD")
    static std::string toDydxSymbol(const std::string& engineSymbol);
    static std::string fromDydxSymbol(const std::string& dydxSymbol);

    mutable std::mutex mutex_; // Protects access to orderBooks_ and wsClients_
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> orderBooks_; // Symbol -> OrderBook;
    std::unordered_map<std::string, std::shared_ptr<ix::WebSocket>> wsClients_; 
    std::unordered_map<std::string, bool> reconnecting_; 
    bool connected_ = false; // Connection status
};