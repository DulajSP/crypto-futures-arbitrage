#include "exchange/DydxTrader.hpp"
#include "common/Logger.hpp"
#include "common/crypto/KeyUtils.hpp"
#include "common/crypto/bech32.hpp"
#include "common/Helper.hpp"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <bip3x/bip3x_mnemonic.h>
#include <bip3x/bip3x_hdkey_encoder.h>
#include <bip3x/bip3x_crypto.h>
#include <bip3x/crypto/sha2.hpp>
#include <bip3x/crypto/ripemd160.h>

#include <cosmos/tx/signing/v1beta1/signing.pb.h>
#include <dydxprotocol/clob/tx.pb.h>
#include <cosmos/tx/v1beta1/tx.pb.h>
#include <google/protobuf/any.pb.h>

#include <sstream>
#include <iomanip>
#include <thread>
#include <regex>

using json = nlohmann::json;
using namespace cosmos::tx::v1beta1;
using namespace dydxprotocol::clob;
using namespace dydxprotocol::subaccounts;

DydxTrader::DydxTrader(std::string mnemonic, 
                        double subaccountNumber)
    : mnemonic_(std::move(mnemonic))
    , subaccountNumber_(subaccountNumber)
{
    deriveKeys();
    getAccountInfo();
    startBlockTrackingThread();
}

void DydxTrader::startBlockTrackingThread() {
    std::thread([this]() { updateBlockHeightLoop(); }).detach();
}

void DydxTrader::updateBlockHeightLoop() {
    while (true) {
        try {
            uint64_t currentBlock = fetchCurrentBlockHeightREST();
            if (currentBlock > 0) {
                lastBlockHeight_ = currentBlock;
                lastBlockTime_ = std::chrono::steady_clock::now();
                Logger::info("[LIVE/DYDX] Updated block height: " + std::to_string(currentBlock));
            }
        } catch (const std::exception& e) {
            Logger::error(std::string("[LIVE/DYDX] Block height fetch error: ") + e.what());
        }

        std::this_thread::sleep_for(std::chrono::minutes(2));
    }
}

uint64_t DydxTrader::fetchCurrentBlockHeightREST() {
    auto [ok, body] = Helper::httpGet(baseURL_, "/cosmos/base/tendermint/v1beta1/blocks/latest", "");
    if (!ok) { Logger::error("[LIVE/DYDX] Failed to fetch latest block: HTTP " + body); return 0; }

    auto j = json::parse(body);
    std::string heightStr = j["block"]["header"]["height"];
    return std::stoull(heightStr);
}

uint64_t DydxTrader::getEstimatedBlockHeight() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastBlockTime_.load()).count();
    return lastBlockHeight_.load() + static_cast<uint64_t>(elapsed / averageBlockTimeSeconds_);
}

void DydxTrader::deriveKeys() {
    // Derive seed and root key
    bip3x::bytes_64 seed = bip3x::bip3x_hdkey_encoder::make_bip39_seed(mnemonic_);
    bip3x::hdkey key = bip3x::bip3x_hdkey_encoder::make_bip32_root_key(seed);
    bip3x::bip3x_hdkey_encoder::extend_key(key, "m/44'/118'/0'/0/0");  

    // Save keys
    privateKey_ = key.private_key.get();
    publicKey_ = key.public_key.get();

    // Generate compressed pubkey and Ethereum-style address
    std::vector<uint8_t> compressedPubKey = KeyUtils::getCompressedPublicKey(privateKey_);
    
    // Generate Bech32 Cosmos address (dydx1...)
    std::vector<uint8_t> shaDigest(32);
    trezor::SHA256_CTX ctx;
    sha256_Init(&ctx);
    sha256_Update(&ctx, compressedPubKey.data(), compressedPubKey.size());
    sha256_Final(&ctx, shaDigest.data());

    std::vector<uint8_t> ripemdDigest(20);
    ripemd160(shaDigest.data(), 32, ripemdDigest.data());

    std::vector<uint8_t> fiveBitAddr;
    const int fromBits = 8;
    const int toBits = 5;
    const int maxv = (1 << toBits) - 1;
    int acc = 0, bits = 0;

    for (uint8_t b : ripemdDigest) {
        acc = (acc << fromBits) | b;
        bits += fromBits;
        while (bits >= toBits) {
            bits -= toBits;
            fiveBitAddr.push_back((acc >> bits) & maxv);
        }
    }

    address_ = bech32::Encode(bech32::Encoding::BECH32, "dydx", fiveBitAddr);
}


void DydxTrader::getAccountInfo() {
    auto [ok, body] = Helper::httpGet(baseURL_, "/cosmos/auth/v1beta1/accounts/" + address_, "");
    if (!ok) { Logger::error("[LIVE/DYDX] Failed to fetch account info: " + body); }

    try {
        auto json = json::parse(body);

        const auto& acc = json["account"];
        accountNumber_ = std::stoull(acc["account_number"].get<std::string>());
        // outSequence = std::stoull(acc["sequence"].get<std::string>());

        Logger::info("[LIVE/DYDX] account_number = " + std::to_string(accountNumber_));
    } catch (const std::exception& e) {
        Logger::error("[LIVE/DYDX] JSON parsing error in getAccountInfo: " + std::string(e.what()));
        Logger::error("[LIVE/DYDX] Raw response: " + body);
        Logger::error("[LIVE/DYDX] Failed to get account info");
    }
}

std::string DydxTrader::signMessage(const std::string& signDocBytes) {
    return KeyUtils::signMessage(signDocBytes, privateKey_);
}

bool DydxTrader::loadSymbolRulesIfNeeded(const std::string& symbol) {
    std::lock_guard<std::mutex> lk(rulesMtx_);
    if (rulesCache_.count(symbol)) return true;
    auto [ok, body] = Helper::httpGet("https://indexer.dydx.trade", "/v4/perpetualMarkets", "");
    if (!ok) { Logger::error("[LIVE/DYDX] exchangeInfo error: " + body); return false; }
    return parseRulesFromExchangeInfo(symbol, body);
}

bool DydxTrader::parseRulesFromExchangeInfo(const std::string& targetSym, const std::string& body) {
    try {
        json j = json::parse(body);
        if (!j.contains("markets")) return false;
        for (auto& [symbol, obj] : j["markets"].items()) {
            if (!obj.contains("clobPairId")) continue;
            if (symbol != targetSym) continue;

            SymbolRules r;
            
            r.clobPairId        = std::stod(obj["clobPairId"].get<std::string>());      
            r.stepSize          = std::stod(obj["stepSize"].get<std::string>());
            r.stepBaseQuantums  = obj["stepBaseQuantums"].get<uint64_t>();
            r.atomicResolution  = obj["atomicResolution"].get<int>();       
            r.subticksPerTick   = obj["subticksPerTick"].get<uint32_t>();

            rulesCache_[targetSym] = r;
            Logger::info("[LIVE/DYDX] Loaded rules for " + targetSym +
                         " atomicResolution=" + std::to_string(r.atomicResolution) +
                         " clobPairId=" + std::to_string(r.clobPairId));
            return true;
        }
    } catch (const std::exception& e) {
        Logger::error(std::string("[LIVE/DYDX] parse exchangeInfo error: ") + e.what());
    }
    return false;
}


bool DydxTrader::clampQtyForSymbol(const std::string& symbol, double reqQty, double& clampedQty, uint64_t& quantums) {
    if (!loadSymbolRulesIfNeeded(symbol)) return false;
    SymbolRules r;
    {
        std::lock_guard<std::mutex> lk(rulesMtx_);
        r = rulesCache_[symbol];
    }

    // Scale by 10^(-atomicResolution)
    double scale = std::pow(10.0, -r.atomicResolution);
    uint64_t rawQuantums = static_cast<uint64_t>(reqQty * scale);

    // Clamp to stepBaseQuantums multiple
    quantums = (rawQuantums / r.stepBaseQuantums) * r.stepBaseQuantums;

    double q = Helper::floorToStep(reqQty, r.stepSize);
    clampedQty = q;

    return quantums > 0;
}

Fill DydxTrader::executeTrade(const std::string& symbol,
                              const std::string& side,
                              double price,
                              double maxQty) {

    Fill fillRes; fillRes.exchange=exchange_; fillRes.symbol=symbol; fillRes.side=side; 
    fillRes.price=price; fillRes.qty=0; fillRes.ts=Helper::nowMs(); fillRes.ok=false;

    if (maxQty <= 0.0) {
        Logger::warn("[LIVE/DYDX] rejected (qty <= 0): " + symbol);
        return fillRes;
    }

    if (accountNumber_ == 0) {
        Logger::warn("[LIVE/DYDX] rejected Account Number not loaded.");
        return fillRes;
    }

    std::string dydxSymbol = Helper::toDydxSymbol(symbol);

    if (!loadSymbolRulesIfNeeded(dydxSymbol)) {
        Logger::warn("[LIVE/DYDX] Failed to load symbol rules for: " + dydxSymbol);
        return fillRes;
    }

    uint32_t clobPairId;
    uint32_t subticksPerTick;

    {
        std::lock_guard<std::mutex> lk(rulesMtx_);
        clobPairId = rulesCache_[dydxSymbol].clobPairId;
        subticksPerTick = rulesCache_[dydxSymbol].subticksPerTick;
    }

    for (int attempt = 1; attempt <= 5; ++attempt) {
        // Build OrderId
        OrderId order_id;
        SubaccountId* sub_id = order_id.mutable_subaccount_id();
        sub_id->set_owner(address_); 
        sub_id->set_number(subaccountNumber_); 
        order_id.set_client_id(std::time(nullptr));
        order_id.set_clob_pair_id(clobPairId);  
        order_id.set_order_flags(0); // set 64 if long term, 32 if conditional order, 0 otherwise

        // Build Order
        Order order;
        *order.mutable_order_id() = order_id;

        const std::string sideUp = Helper::upper(side);
        order.set_side(sideUp == "BUY" ? Order_Side_SIDE_BUY : Order_Side_SIDE_SELL);

        double sendQty = 0.0; uint64_t quantums = 0;
        
        if (!clampQtyForSymbol(dydxSymbol, maxQty, sendQty, quantums)) {
            Logger::warn("[LIVE/DYDX] qty too small after clamping: req=" + std::to_string(maxQty));
            return fillRes;
        }
        // if (sendQty != maxQty) {
        //     Logger::info("[LIVE/DYDX] qty adjusted from " + std::to_string(maxQty) + " -> " + std::to_string(sendQty));
        // }
        
        order.set_quantums(quantums);

        // order.set_client_metadata(1);
        order.set_reduce_only(false); 
        order.set_good_til_block(getEstimatedBlockHeight() + 15);
        // order.set_good_til_block_time(1765577533);
        order.set_subticks(subticksPerTick);
        // order.set_time_in_force(dydxprotocol::clob::Order_TimeInForce_TIME_IN_FORCE_UNSPECIFIED)

        // Wrap into MsgPlaceOrder
        MsgPlaceOrder msg;
        *msg.mutable_order() = order;

        // Create TxBody
        cosmos::tx::v1beta1::TxBody txBody;
        google::protobuf::Any* msgAny = txBody.add_messages();
        msgAny->set_type_url("/dydxprotocol.clob.MsgPlaceOrder");
        msg.SerializeToString(msgAny->mutable_value());
        txBody.set_memo("");

        // AuthInfo
        cosmos::tx::v1beta1::AuthInfo authInfo;
        auto* signerInfo = authInfo.add_signer_infos();
        google::protobuf::Any pubkeyAny;
        KeyUtils::setPubkeyAny(pubkeyAny, KeyUtils::getCompressedPublicKey(privateKey_));
        *signerInfo->mutable_public_key() = pubkeyAny;
        signerInfo->mutable_mode_info()->mutable_single()->set_mode(
            cosmos::tx::signing::v1beta1::SIGN_MODE_DIRECT
        );
        signerInfo->set_sequence(0);
        auto* fee = authInfo.mutable_fee();
        fee->set_gas_limit(200000);
        auto* amount = fee->add_amount();
        amount->set_denom("usdc");
        amount->set_amount("0");

        // SignDoc
        cosmos::tx::v1beta1::SignDoc signDoc;
        std::string txBodyBytes, authInfoBytes;
        txBody.SerializeToString(&txBodyBytes);
        authInfo.SerializeToString(&authInfoBytes);
        signDoc.set_body_bytes(txBodyBytes);
        signDoc.set_auth_info_bytes(authInfoBytes);
        signDoc.set_chain_id("dydx-mainnet-1");
        signDoc.set_account_number(accountNumber_);

        std::string sig = signMessage(signDoc.SerializeAsString());

        // TxRaw
        cosmos::tx::v1beta1::TxRaw txRaw;
        txRaw.set_body_bytes(txBodyBytes);
        txRaw.set_auth_info_bytes(authInfoBytes);
        txRaw.add_signatures(sig);

        std::string txRawBase64 = KeyUtils::base64Encode(txRaw.SerializeAsString());

        json body = {
            {"tx_bytes", txRawBase64},
            {"mode", "BROADCAST_MODE_SYNC"}
        };

        auto [ok, respText] = Helper::httpPost(baseURL_ + "/cosmos/tx/v1beta1/txs", body.dump(), {});
        
        try {
            auto j = json::parse(respText);
            if (ok && j.contains("tx_response") && j["tx_response"].contains("code") && j["tx_response"]["code"] == 0) { 
                Logger::info("[LIVE/DYDX] " + sideUp + " " + symbol +
                         " qty=" + std::to_string(sendQty) + " @ ~" + std::to_string(price));

                fillRes.qty = sendQty; fillRes.cost = sendQty * price; fillRes.ok = true;
                return fillRes; 
                
            } else if (j.contains("tx_response") && j["tx_response"].contains("raw_log")) {
                const std::string& rawLog = j["tx_response"]["raw_log"].get<std::string>();

                std::regex rgx(R"(current blockHeight (\d+))");
                std::smatch match;

                if (std::regex_search(rawLog, match, rgx)) {
                    int64_t block = std::stoll(match[1].str());
                    if (block > 0) {
                        lastBlockHeight_ = block;
                        lastBlockTime_ = std::chrono::steady_clock::now();
                        Logger::info("[LIVE/DYDX] Synced block height: " + std::to_string(block));
                    }
                } 
            } 
        } catch (const std::exception& e) {
            Logger::error(std::string("[LIVE/BINANCE] parse error: ") + e.what() + " body=" + respText);
        }

        Logger::error("[LIVE/DYDX] order failed Retrying: " + respText);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    Logger::error("[LIVE/DYDX] order failed after 5 attempts");
    return fillRes;
}