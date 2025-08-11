#include "core/PaperTrader.hpp"
#include "common/Logger.hpp"
#include <chrono>

static int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

PaperTrader::PaperTrader(std::string exchangeName, double feePercent)
    : exchange_(std::move(exchangeName)), feePct_(feePercent) {}

Fill PaperTrader::executeTrade(
    const std::string& symbol,
    const std::string& side,
    double price,
    double maxQty
) {
    Fill f;
    f.exchange  = exchange_;
    f.symbol = symbol;
    f.side   = side;
    f.price  = price;
    f.qty    = maxQty; 
    f.cost   = std::round(f.qty * f.price * 100.0) / 100.0; // usd amount Round to 2 decimals
    f.fee    = (price * f.qty) * (feePct_ / 100.0);
    f.ts     = now_ms();
    f.ok     = (f.qty > 0.0 && f.price > 0.0);

    if (f.ok) {
        Logger::info("[PAPER/" + exchange_ + "] " + side + " " + symbol +
                     " qty=" + std::to_string(f.qty) +
                     " @ " + std::to_string(f.price) +
                     " fee=" + std::to_string(f.fee));
    } else {
        Logger::info("[PAPER/" + exchange_ + "] rejected " + side + " " + symbol);
    }
    return f;
}