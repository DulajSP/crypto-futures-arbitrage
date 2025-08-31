#include "core/Watchdog.hpp"

using namespace std::chrono;

Watchdog::Watchdog() {}
Watchdog::~Watchdog() { stop(); }

void Watchdog::start() {
    if (running_) return;
    running_ = true;
    loopThread_ = std::thread(&Watchdog::run, this);
}

void Watchdog::stop() {
    if (!running_) return;
    running_ = false;
    if (loopThread_.joinable()) loopThread_.join();
}

void Watchdog::registerMarketClient(const std::shared_ptr<IExchangeClient>& client,
                                    const std::vector<std::string>& symbols) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string exName = client->getExchangeName();
    clients_[exName] = client;

    auto now = steady_clock::now();
    for (const auto& s : symbols) {
        FeedKey key{exName, "orderbook", s};
        lastUpdate_[key] = now;
    }
}

void Watchdog::registerPrivateClient(const std::shared_ptr<IExchangeClient>& client,
                                     const std::string& channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string exName = client->getExchangeName();
    clients_[exName] = client;

    auto now = steady_clock::now();
    FeedKey key{exName, channel, ""};
    lastUpdate_[key] = now;
}

void Watchdog::markUpdate(const std::string& ex, const std::string& channel,
                          const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    FeedKey key{ex, channel, symbol};
    lastUpdate_[key] = steady_clock::now();
}

void Watchdog::run() {
    const auto invalidGrace = minutes(5);
    const auto staleOrderbook = minutes(5);
    const auto stalePrivate = minutes(30);

    while (running_) {
        try {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto now = steady_clock::now();

                for (auto& [exName, client] : clients_) {
                    for (auto& [key, ts] : lastUpdate_) {
                        if (key.exchange != exName) continue;

                        auto dur = duration_cast<seconds>(now - ts);

                        if (key.channel == "orderbook") {
                            auto ob = client->getOrderBook(key.symbol);
                            if (ob) {
                                double bid = ob->getTopBidPrice();
                                double ask = ob->getTopAskPrice();

                                if (bid > 0 && ask > 0 && bid >= ask) {
                                    if (!invalidSince_.count(key)) {
                                        invalidSince_[key] = now;
                                        Logger::warn("Watchdog: invalid book " +
                                                    key.exchange + " " + key.symbol);
                                    } else {
                                        auto badFor = duration_cast<minutes>(now - invalidSince_[key]);
                                        if (badFor >= invalidGrace) {
                                            Logger::warn("Watchdog: book still invalid after wait, reconnecting.. "
                                                        + key.exchange + " " + key.symbol);
                                            client->requestReconnect(key.symbol);
                                            invalidSince_.erase(key);
                                        }
                                    }
                                } else {
                                    invalidSince_.erase(key);
                                }
                            }

                            if (dur >= staleOrderbook) {
                                Logger::warn("Watchdog: stale orderbook, reconnecting "
                                            + key.exchange + " " + key.symbol);
                                client->requestReconnect(key.symbol);
                                lastUpdate_[key] = now;
                            }
                        } else {
                            // private feeds (orders, account)
                            if (dur >= stalePrivate) {
                                Logger::warn("Watchdog: stale private feed, reconnecting "
                                            + key.exchange + " channel=" + key.channel);
                                client->requestReconnect(""); // reconnect whole feed
                                lastUpdate_[key] = now;
                            }
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            Logger::error(std::string("Watchdog exception: ") + e.what());
        } catch (...) {
            Logger::error("Watchdog exception: unknown");
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}