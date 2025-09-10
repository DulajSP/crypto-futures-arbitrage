#include "exchange/BybitTrader.hpp"
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

BybitTrader::BybitTrader(std::string apiKey,
                         std::string apiSecret)
    : apiKey_(std::move(apiKey))
    , apiSecret_(std::move(apiSecret))
{}

std::string BybitTrader::hmacSha256Hex(const std::string& secret, const std::string& msg) {
    unsigned int len = 0; unsigned char mac[EVP_MAX_MD_SIZE];
    HMAC(EVP_sha256(), secret.data(), (int)secret.size(),
         reinterpret_cast<const unsigned char*>(msg.data()), msg.size(), mac, &len);
    std::ostringstream oss;
    for (unsigned int i=0;i<len;++i) oss<<std::hex<<std::setw(2)<<std::setfill('0')<<(int)mac[i];
    return oss.str();
}


bool BybitTrader::loadSymbolRulesIfNeeded(const std::string& symbol) {
    std::lock_guard<std::mutex> lk(rulesMtx_);
    if (rulesCache_.count(symbol)) return true;

    auto [ok, body] = Helper::httpGet(baseURL_, "/v5/market/instruments-info",
                              "category=linear&symbol=" + symbol);
    if (!ok) {
        Logger::error("[LIVE/BYBIT] instruments-info error: " + body);
        return false;
    }
    return parseRulesFromInstrumentsInfo(symbol, body);
}

bool BybitTrader::parseRulesFromInstrumentsInfo(const std::string& targetSym, const std::string& body) {
    try {
        auto j = json::parse(body);
        if (!j.contains("retCode") || j["retCode"].get<int>() != 0) return false;
        if (!j.contains("result") || !j["result"].contains("list")) return false;

        for (const auto& it : j["result"]["list"]) {
            if (!it.contains("symbol")) continue;
            if (it["symbol"].get<std::string>() != targetSym) continue;

            SymbolRules r;

            // qtyStep/minOrderQty under lotSizeFilter
            if (it.contains("lotSizeFilter")) {
                const auto& lf = it["lotSizeFilter"];
                if (lf.contains("qtyStep"))     r.qtyStep = std::stod(lf["qtyStep"].get<std::string>());
                if (lf.contains("minOrderQty")) r.minQty  = std::stod(lf["minOrderQty"].get<std::string>());
                // derive dp
                if (lf.contains("qtyStep")) {
                    auto ss = lf["qtyStep"].get<std::string>();
                    auto dot = ss.find('.');
                    if (dot != std::string::npos) r.qtyDp = (int)(ss.size() - dot - 1);
                }
            }

            if (r.qtyStep <= 0.0) { r.qtyStep = 0.000001; r.minQty = 0.0; }

            rulesCache_[targetSym] = r;
            Logger::info("[LIVE/BYBIT] Loaded rules for " + targetSym +
                         " step=" + std::to_string(r.qtyStep) +
                         " minQty=" + std::to_string(r.minQty));
            return true;
        }
    } catch (const std::exception& e) {
        Logger::error(std::string("[LIVE/BYBIT] parse instruments-info error: ") + e.what());
    }
    return false;
}

bool BybitTrader::clampQtyForSymbol(const std::string& symbol, double reqQty, double& clampedQty, std::string& qtyStr) {
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


Fill BybitTrader::executeTrade(const std::string& symbol,
                               const std::string& side,
                               double price,
                               double maxQty)
{
    Fill fillRes; fillRes.exchange=exchange_; fillRes.symbol=symbol; fillRes.side=side; 
    fillRes.price=price; fillRes.qty=0; fillRes.ts=Helper::nowMs(); fillRes.ok=false;

    if (maxQty <= 0.0) { Logger::warn("[LIVE/BYBIT] rejected (qty<=0): " + symbol); return fillRes; }
    const std::string sideUp = Helper::upper(side);
    if (sideUp!="BUY" && sideUp!="SELL") { Logger::error("[LIVE/BYBIT] invalid side: " + side); return fillRes; }

    // Clamp quantity to lotSizeFilter
    double sendQty = 0.0; std::string qtyStr;
    if (!clampQtyForSymbol(symbol, maxQty, sendQty, qtyStr)) {
        Logger::warn("[LIVE/BYBIT] qty too small after clamping: req=" + std::to_string(maxQty));
        return fillRes;
    }
    // if (sendQty != maxQty) {
    //     Logger::info("[LIVE/BYBIT] qty adjusted from " + std::to_string(maxQty) + " -> " + std::to_string(sendQty));
    // }

    for (int attempt = 1; attempt <= 5; ++attempt) {
        // Build v5 order body
        nlohmann::json body = {
            {"category",  "linear"},
            {"symbol",    symbol},
            {"side",      sideUp == "BUY" ? "Buy" : "Sell"},
            {"orderType", "Market"},
            {"qty",       qtyStr}
            //  {"timeInForce","IOC"}, {"reduceOnly",false}, {"positionIdx",1/2}
        };
        const std::string bodyStr = body.dump();

        const std::string ts = std::to_string(Helper::nowMs());
        const std::string prehash = ts + apiKey_ + "5000" + bodyStr;
        const std::string sign = hmacSha256Hex(apiSecret_, prehash);

        auto [ok, respText] = Helper::httpPost(
            baseURL_ + "/v5/order/create",
            bodyStr,
            {
                {"X-BAPI-API-KEY", apiKey_},
                {"X-BAPI-SIGN", sign},
                {"X-BAPI-SIGN-TYPE", "2"},
                {"X-BAPI-TIMESTAMP", ts},
                {"X-BAPI-RECV-WINDOW", "5000"}
            }
        );
        try {
            auto j = json::parse(respText);
            if (ok && !(!j.contains("retCode") || j["retCode"].get<int>() != 0)) { 
                Logger::info("[LIVE/BYBIT] " + sideUp + " " + symbol +
                         " qty=" + std::to_string(sendQty) + " @ ~" + std::to_string(price));

                fillRes.qty = sendQty; fillRes.cost = sendQty * price; fillRes.ok = true;
                return fillRes; 
            } 
        } catch (const std::exception& e) {
            Logger::error(std::string("[LIVE/BYBIT] parse error: ") + e.what() + " body=" + respText);
        }

        Logger::error("[LIVE/BYBIT] order failed Retrying: " + respText);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    Logger::error("[LIVE/BYBIT] order failed after 5 attempts");
    return fillRes;
}