#include "exchange/BinanceTrader.hpp"
#include "common/Helper.hpp"

#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>

#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cmath>

using nlohmann::json;


BinanceTrader::BinanceTrader(std::string apiKey,
                             std::string apiSecret)
    : apiKey_(std::move(apiKey))
    , apiSecret_(std::move(apiSecret))
{}

std::string BinanceTrader::signQuery(const std::string& qs) const {
    unsigned int len = 0;
    unsigned char mac[EVP_MAX_MD_SIZE];
    HMAC(EVP_sha256(), apiSecret_.data(), (int)apiSecret_.size(),
         reinterpret_cast<const unsigned char*>(qs.data()), qs.size(),
         mac, &len);
    std::ostringstream oss;
    for (unsigned int i=0;i<len;++i) oss<<std::hex<<std::setw(2)<<std::setfill('0')<<(int)mac[i];
    return oss.str();
}

bool BinanceTrader::loadSymbolRulesIfNeeded(const std::string& symbol) {
    std::lock_guard<std::mutex> lk(rulesMtx_);
    if (rulesCache_.count(symbol)) return true;
    auto [ok, body] = Helper::httpGet(baseURL_, "/fapi/v1/exchangeInfo", "");
    if (!ok) { Logger::error("[LIVE/BINANCE] exchangeInfo error: " + body); return false; }
    return parseRulesFromExchangeInfo(symbol, body);
}

bool BinanceTrader::parseRulesFromExchangeInfo(const std::string& targetSym, const std::string& body) {
    try {
        json j = json::parse(body);
        if (!j.contains("symbols")) return false;
        for (const auto& s : j["symbols"]) {
            if (!s.contains("symbol")) continue;
            if (s["symbol"].get<std::string>() != targetSym) continue;

            SymbolRules r;
            // default dp
            r.qtyDp = 6;

            if (s.contains("filters")) {
                for (const auto& f : s["filters"]) {
                    if (!f.contains("filterType")) continue;
                    std::string ft = f["filterType"].get<std::string>();
                    if (ft == "LOT_SIZE") {
                        if (f.contains("stepSize")) r.qtyStep = std::stod(f["stepSize"].get<std::string>());
                        if (f.contains("minQty"))   r.minQty  = std::stod(f["minQty"].get<std::string>());
                        if (f.contains("maxQty"))   r.maxQty  = std::stod(f["maxQty"].get<std::string>());
                        // derive dp from stepSize string like "0.001"
                        auto ss = f["stepSize"].get<std::string>();
                        auto dot = ss.find('.');
                        if (dot != std::string::npos) r.qtyDp = (int)(ss.size() - dot - 1);
                    }
                }
            }

            if (r.qtyStep <= 0.0) {
                // Fallback
                r.qtyStep = 0.000001;
                r.minQty  = 0.0;
            }

            rulesCache_[targetSym] = r;
            Logger::info("[LIVE/BINANCE] Loaded rules for " + targetSym +
                         " step=" + std::to_string(r.qtyStep) +
                         " minQty=" + std::to_string(r.minQty));
            return true;
        }
    } catch (const std::exception& e) {
        Logger::error(std::string("[LIVE/BINANCE] parse exchangeInfo error: ") + e.what());
    }
    return false;
}

bool BinanceTrader::clampQtyForSymbol(const std::string& symbol, double reqQty, double& clampedQty, std::string& qtyStr) {
    if (!loadSymbolRulesIfNeeded(symbol)) return false;
    SymbolRules r;
    {
        std::lock_guard<std::mutex> lk(rulesMtx_);
        r = rulesCache_[symbol];
    }
    double q = Helper::floorToStep(reqQty, r.qtyStep);
    if (q < r.minQty) q = 0.0;

    clampedQty = q;
    if (q > 0.0) qtyStr = Helper::trimTrailingZeros(Helper::toFixed(q, std::min(r.qtyDp, 8)));
    return q > 0.0;
}

Fill BinanceTrader::executeTrade(const std::string& symbol,
                                 const std::string& side,
                                 double price,
                                 double maxQty)
{
    Fill fillRes; fillRes.exchange=exchange_; fillRes.symbol=symbol; fillRes.side=side; 
    fillRes.price=price; fillRes.qty=0; fillRes.ts=Helper::nowMs(); fillRes.ok=false;

    if (maxQty <= 0.0) { Logger::warn("[LIVE/BINANCE] rejected (qty<=0): " + symbol); return fillRes; }
    const std::string sideUp = Helper::upper(side);
    if (sideUp!="BUY" && sideUp!="SELL") { Logger::error("[LIVE/BINANCE] invalid side: " + side); return fillRes; }

    // Clamp quantity to LOT_SIZE
    double sendQty = 0.0; std::string qtyStr;
    if (!clampQtyForSymbol(symbol, maxQty, sendQty, qtyStr)) {
        Logger::warn("[LIVE/BINANCE] qty too small after clamping: req=" + std::to_string(maxQty));
        return fillRes;
    }
    // if (sendQty != maxQty) {
    //     Logger::info("[LIVE/BINANCE] qty adjusted from " + std::to_string(maxQty) + " -> " + std::to_string(sendQty));
    // }

    for (int attempt = 1; attempt <= 5; ++attempt) {
        const int64_t ts = Helper::nowMs();
        std::ostringstream qs;
        qs << "symbol=" << symbol
        << "&side=" << sideUp
        << "&type=MARKET"
        << "&quantity=" << qtyStr
        << "&recvWindow=5000"
        << "&timestamp=" << ts;

        const std::string signature = signQuery(qs.str());
        std::string body = qs.str() + "&signature=" + signature;


        auto [ok, respText] = Helper::httpPost(baseURL_ + "/fapi/v1/order", 
            body,
            {
                {"X-MBX-APIKEY", apiKey_}
            },
            "application/x-www-form-urlencoded");
        
        try {
            auto j = json::parse(respText);
            if (ok && !(j.contains("code") && j["code"].get<int>() != 0)) { 
                Logger::info("[LIVE/BINANCE] " + sideUp + " " + symbol +
                         " qty=" + std::to_string(sendQty) + " @ ~" + std::to_string(price));

                fillRes.qty = sendQty; fillRes.cost = sendQty * price; fillRes.ok = true;
                return fillRes; 
            } 
        } catch (const std::exception& e) {
            Logger::error(std::string("[LIVE/BINANCE] parse error: ") + e.what() + " body=" + respText);
        }

        Logger::error("[LIVE/BINANCE] order failed Retrying: " + respText);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    Logger::error("[LIVE/BINANCE] order failed after 5 attempts");
    return fillRes;
}

