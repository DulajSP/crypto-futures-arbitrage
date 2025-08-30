#pragma once

#include <map>
#include <vector>
#include <mutex>
#include <functional>

// Thread-safe order book for managing bids and asks.
class OrderBook {
public:
    OrderBook();

    using PriceLevel = std::pair<double, double>;  // (price, quantity)
    using Bids = std::map<double, double, std::greater<>>; // highest -> lowest
    using Asks = std::map<double, double, std::less<>>;    // lowest  -> highest

    // Update or remove a price level.
    void updateBid(double price, double quantity);
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
    Bids bids_;  // Bid side order book
    Asks asks_;  // Ask side order book
    mutable std::mutex mutex_;  // Protects order book for thread safety
};