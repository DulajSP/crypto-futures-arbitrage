#include "common/crypto/KeyUtils.hpp"
#include <secp256k1.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <google/protobuf/any.pb.h>
#include <cosmos/crypto/secp256k1/keys.pb.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <string>
#include <cstring>
#include <openssl/evp.h>
#include <openssl/buffer.h>

static secp256k1_context* secp256k1_ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);

std::vector<uint8_t> KeyUtils::getCompressedPublicKey(const std::vector<uint8_t>& privateKey) {
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    secp256k1_pubkey pubkey;

    if (!secp256k1_ec_pubkey_create(ctx, &pubkey, privateKey.data())) {
        throw std::runtime_error("Failed to create public key");
    }

    std::vector<uint8_t> compressed(33);
    size_t len = compressed.size();
    secp256k1_ec_pubkey_serialize(ctx, compressed.data(), &len, &pubkey, SECP256K1_EC_COMPRESSED);

    secp256k1_context_destroy(ctx);
    return compressed;
}

std::string KeyUtils::getEthereumAddress(const std::vector<uint8_t>& compressedPubKey) {
    std::vector<uint8_t> uncompressed(65);
    size_t len = 65;

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    secp256k1_pubkey pubkey;
    secp256k1_ec_pubkey_parse(ctx, &pubkey, compressedPubKey.data(), compressedPubKey.size());
    secp256k1_ec_pubkey_serialize(ctx, uncompressed.data(), &len, &pubkey, SECP256K1_EC_UNCOMPRESSED);
    secp256k1_context_destroy(ctx);

    uint8_t hash[32];
    SHA256(uncompressed.data() + 1, 64, hash);  // skip 0x04 prefix

    uint8_t ripemd[20];
    RIPEMD160(hash, 32, ripemd);

    std::ostringstream oss;
    for (int i = 0; i < 20; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ripemd[i]);
    }
    return oss.str();
}

std::string KeyUtils::base64Encode(const std::string& input) {
    BIO* bio, *b64;
    BUF_MEM* bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input.data(), input.size());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string encoded(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    return encoded;
}

std::string KeyUtils::signMessage(const std::string& signDocBytes, const std::vector<uint8_t>& privateKey) {
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);

    uint8_t hash[32];
    SHA256(reinterpret_cast<const uint8_t*>(signDocBytes.data()), signDocBytes.size(), hash);

    secp256k1_ecdsa_signature signature;
    if (!secp256k1_ecdsa_sign(ctx, &signature, hash, privateKey.data(), nullptr, nullptr)) {
        secp256k1_context_destroy(ctx);
        throw std::runtime_error("Failed to sign message");
    }

    std::vector<uint8_t> compactSig(64);
    secp256k1_ecdsa_signature_serialize_compact(ctx, compactSig.data(), &signature);
    secp256k1_context_destroy(ctx);

    return std::string(compactSig.begin(), compactSig.end());
}

void KeyUtils::setPubkeyAny(google::protobuf::Any& anyPubkey, const std::vector<uint8_t>& compressedPubKey) {
    cosmos::crypto::secp256k1::PubKey pubkeyProto;
    pubkeyProto.set_key(std::string(compressedPubKey.begin(), compressedPubKey.end()));
    anyPubkey.set_type_url("/cosmos.crypto.secp256k1.PubKey");
    pubkeyProto.SerializeToString(anyPubkey.mutable_value());
}