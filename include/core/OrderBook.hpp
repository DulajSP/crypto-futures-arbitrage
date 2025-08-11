#pragma once

#include <map>
#include <vector>
#include <mutex>

// Thread-safe order book for managing bids and asks.
class OrderBook {
public:
    OrderBook();

    using PriceLevel = std::pair<double, double>;  // (price, quantity)
    using BookSide = std::map<double, double, std::greater<>>;  // price -> quantity, descending order for bids

    // Update or remove a bid price level.
    void updateBid(double price, double quantity);

    // Update or remove an ask price level.
    void updateAsk(double price, double quantity);

    // Return top N bids (highest price first).
    std::vector<PriceLevel> getTopNBids(size_t n) const;

    // Return top N asks (lowest price first).
    std::vector<PriceLevel> getTopNAsks(size_t n) const;

    // Get best (highest) bid price.
    double getTopBidPrice() const;

    // Get best (lowest) ask price.
    double getTopAskPrice() const;

    // Get quantity at best bid price.
    double getTopBidQty() const;   

    // Get quantity at best ask price.
    double getTopAskQty() const;   

    // Remove all bids and asks.
    void clear();

private:
    BookSide bids_;  // Bid side order book
    BookSide asks_;  // Ask side order book
    mutable std::mutex mutex_;  // Protects order book for thread safety
};