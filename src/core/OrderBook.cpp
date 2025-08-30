#include "core/OrderBook.hpp"

OrderBook::OrderBook() {}

void OrderBook::updateBid(double price, double qty) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (qty == 0.0) bids_.erase(price); // Remove level if qty is zero
    else bids_[price] = qty;            // Insert or update bid
}

void OrderBook::updateAsk(double price, double qty) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (qty == 0.0) asks_.erase(price); // Remove level if qty is zero
    else asks_[price] = qty;            // Insert or update ask
}

void OrderBook::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    bids_.clear();
    asks_.clear();
}

std::vector<OrderBook::PriceLevel> OrderBook::getTopNBids(size_t n) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PriceLevel> result;
    result.reserve(n);
    for (const auto& [price, qty] : bids_) {
        if (qty > 0.0) result.emplace_back(price, qty);
        if (result.size() >= n) break;
    }
    return result;
}

std::vector<OrderBook::PriceLevel> OrderBook::getTopNAsks(size_t n) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PriceLevel> result;
    result.reserve(n);
    for (const auto& [price, qty] : asks_) {
        if (qty > 0.0) result.emplace_back(price, qty);
        if (result.size() >= n) break;
    }
    return result;
}

double OrderBook::getTopBidPrice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bids_.empty() ? 0.0 : bids_.begin()->first;
}

double OrderBook::getTopAskPrice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return asks_.empty() ? 0.0 : asks_.begin()->first;
}

double OrderBook::getTopBidQty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bids_.empty() ? 0.0 : bids_.begin()->second;
}

double OrderBook::getTopAskQty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return asks_.empty() ? 0.0 : asks_.begin()->second;
}