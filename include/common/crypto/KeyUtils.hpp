#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <google/protobuf/any.pb.h>

// Provides cryptographic key utilities.
namespace KeyUtils {

    // Returns compressed public key from private key.
    std::vector<uint8_t> getCompressedPublicKey(const std::vector<uint8_t>& privateKey);

    // Returns Ethereum address from compressed public key.
    std::string getEthereumAddress(const std::vector<uint8_t>& compressedPubKey);

    // Encodes input string to Base64.
    std::string base64Encode(const std::string& input);

    // Signs message using private key.
    std::string signMessage(const std::string& signDocBytes, const std::vector<uint8_t>& privateKey);

    // Sets protobuf Any with compressed public key.
    void setPubkeyAny(google::protobuf::Any& anyPubkey, const std::vector<uint8_t>& compressedPubKey);

}