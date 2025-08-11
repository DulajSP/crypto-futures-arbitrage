#pragma once

#include <string>
#include <memory>
#include "core/OrderBook.hpp"

// Interface for exchange clients.
class IExchangeClient {
public:
    virtual ~IExchangeClient() = default;

    // Establish connection to the exchange.
    virtual void connect() = 0;

    // Disconnect from the exchange.
    virtual void disconnect() = 0;

    // Subscribe to order book updates for a symbol.
    virtual void subscribeOrderBook(const std::string& symbol) = 0;

    // Get the current order book for a symbol.
    virtual std::shared_ptr<OrderBook> getOrderBook(const std::string& symbol) const = 0;

    // Returns the exchange name (e.g., "binance_futures").
    virtual std::string getExchangeName() const = 0;
};