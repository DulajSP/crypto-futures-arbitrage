#pragma once

#include "exchange/IExchangeClient.hpp"
#include "core/OrderBook.hpp"
#include "common/Logger.hpp"

#include <unordered_map>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>

// Identify a feed uniquely: exchange + channel + optional symbol
struct FeedKey {
    std::string exchange;
    std::string channel; // e.g. "orderbook", "orders", "account"
    std::string symbol;  // empty for non-symbol feeds

    bool operator==(const FeedKey& other) const {
        return exchange == other.exchange &&
               channel == other.channel &&
               symbol == other.symbol;
    }
};

// Hash function for FeedKey
struct FeedKeyHash {
    std::size_t operator()(const FeedKey& k) const {
        return std::hash<std::string>()(k.exchange + ":" + k.channel + ":" + k.symbol);
    }
};

class Watchdog {
public:
    Watchdog();
    ~Watchdog();

    void start();
    void stop();

    // Register an exchange client + symbols for market data
    void registerMarketClient(const std::shared_ptr<IExchangeClient>& client,
                              const std::vector<std::string>& symbols);

    // Register a private feed (like "orders" or "account") without symbols
    void registerPrivateClient(const std::shared_ptr<IExchangeClient>& client,
                               const std::string& channel);

    // Called when a new update is processed
    void markUpdate(const std::string& exchange, const std::string& channel,
                    const std::string& symbol = "");

private:
    void run();

    // Registered clients
    std::unordered_map<std::string, std::shared_ptr<IExchangeClient>> clients_;

    // Last update times
    std::unordered_map<FeedKey, std::chrono::steady_clock::time_point, FeedKeyHash> lastUpdate_;

    // Invalid book detection (for orderbooks only)
    std::unordered_map<FeedKey, std::chrono::steady_clock::time_point, FeedKeyHash> invalidSince_;

    std::thread loopThread_;
    std::atomic<bool> running_{false};
    std::mutex mutex_;
};