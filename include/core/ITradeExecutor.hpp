#pragma once
#include <string>
#include <cstdint>

// Trade fill report structure.
struct Fill {
    std::string exchange;   // Exchange name (e.g., "Binance Futures", "Bybit Futures").
    std::string symbol;     // Trading symbol (e.g., "BTCUSDT").
    std::string side;       // Trade side: "buy" or "sell".
    double price = 0.0;     // Executed price.
    double qty   = 0.0;     // Executed base-asset quantity.
    double cost  = 0.0;     // Total cost in quote currency (e.g., USDT).
    double fee   = 0.0;     // Fee in quote currency.
    int64_t ts   = 0;       // Execution timestamp (epoch ms).
    bool ok      = false;   // True if trade was successful.
};

// Interface for trade execution on an exchange.
class ITradeExecutor {
public:
    virtual ~ITradeExecutor() = default; // Ensure proper cleanup in derived classes.

    // Execute a single trade and return a fill report.
    // price: reference/limit price (PaperExecutor will execute at this; Live will use average).
    // maxQty: maximum quantity to trade.
    virtual Fill executeTrade(
        const std::string& symbol,
        const std::string& side,   // "buy" or "sell"
        double price,
        double maxQty
    ) = 0;
};