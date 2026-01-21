// Minimal WASM API for Hive beekeeper
// Only depends on secp256k1 - no Boost, no OpenSSL, no FC

#include <emscripten/bind.h>
#include <secp256k1.h>
#include <secp256k1_recovery.h>

#include "base58.hpp"

#include <string>
#include <vector>
#include <array>
#include <optional>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace {

// Global secp256k1 context (created once)
secp256k1_context* get_context() {
    static secp256k1_context* ctx = secp256k1_context_create(
        SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY
    );
    return ctx;
}

// Convert bytes to hex string
std::string bytes_to_hex(const uint8_t* data, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

// Convert hex string to bytes
std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    if (hex.length() % 2 != 0) return bytes;

    bytes.reserve(hex.length() / 2);
    for (size_t i = 0; i < hex.length(); i += 2) {
        uint8_t byte = 0;
        for (int j = 0; j < 2; ++j) {
            char c = hex[i + j];
            byte <<= 4;
            if (c >= '0' && c <= '9') byte |= c - '0';
            else if (c >= 'a' && c <= 'f') byte |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') byte |= c - 'A' + 10;
            else return {};
        }
        bytes.push_back(byte);
    }
    return bytes;
}

// WIF (Wallet Import Format) version byte
constexpr uint8_t WIF_VERSION = 0x80;

// Parse WIF to get raw private key (32 bytes)
std::optional<std::array<uint8_t, 32>> parse_wif(const std::string& wif) {
    auto decoded = minimal::from_base58(wif);
    if (decoded.empty()) return std::nullopt;

    // WIF format: version (1) + key (32) + [compressed flag (1)] + checksum (4)
    // Minimum: 1 + 32 + 4 = 37 bytes
    // With compressed flag: 1 + 32 + 1 + 4 = 38 bytes
    if (decoded.size() != 37 && decoded.size() != 38) return std::nullopt;

    // Check version byte
    if (decoded[0] != WIF_VERSION) return std::nullopt;

    // Verify checksum (double SHA-256)
    size_t data_len = decoded.size() - 4;
    auto checksum = minimal::double_sha256(decoded.data(), data_len);

    if (std::memcmp(checksum.data(), decoded.data() + data_len, 4) != 0) {
        return std::nullopt;
    }

    // Extract 32-byte private key
    std::array<uint8_t, 32> key;
    std::copy(decoded.begin() + 1, decoded.begin() + 33, key.begin());
    return key;
}

// Get compressed public key from private key
std::optional<std::array<uint8_t, 33>> get_public_key(const uint8_t* privkey) {
    auto ctx = get_context();
    secp256k1_pubkey pubkey;

    if (!secp256k1_ec_pubkey_create(ctx, &pubkey, privkey)) {
        return std::nullopt;
    }

    std::array<uint8_t, 33> serialized;
    size_t len = serialized.size();

    if (!secp256k1_ec_pubkey_serialize(ctx, serialized.data(), &len, &pubkey,
                                        SECP256K1_EC_COMPRESSED)) {
        return std::nullopt;
    }

    return serialized;
}

// Format public key with STM prefix (or custom prefix)
std::string format_public_key(const std::array<uint8_t, 33>& pubkey, const std::string& prefix) {
    // Calculate RIPEMD160 checksum (simplified: use last 4 bytes of double SHA256)
    auto checksum = minimal::double_sha256(pubkey.data(), pubkey.size());

    // Combine pubkey + checksum (4 bytes)
    std::vector<uint8_t> with_checksum(pubkey.begin(), pubkey.end());
    with_checksum.insert(with_checksum.end(), checksum.begin(), checksum.begin() + 4);

    return prefix + minimal::to_base58(with_checksum.data(), with_checksum.size());
}

// Parse public key string (with prefix) to raw bytes
std::optional<std::array<uint8_t, 33>> parse_public_key(const std::string& pubkey_str, const std::string& prefix) {
    if (pubkey_str.substr(0, prefix.length()) != prefix) {
        return std::nullopt;
    }

    auto decoded = minimal::from_base58(pubkey_str.substr(prefix.length()));
    if (decoded.size() != 37) return std::nullopt; // 33 + 4 checksum

    // Verify checksum
    auto checksum = minimal::double_sha256(decoded.data(), 33);
    if (std::memcmp(checksum.data(), decoded.data() + 33, 4) != 0) {
        return std::nullopt;
    }

    std::array<uint8_t, 33> result;
    std::copy(decoded.begin(), decoded.begin() + 33, result.begin());
    return result;
}

} // anonymous namespace

// ============================================================================
// Exported WASM API functions
// ============================================================================

/**
 * Parse WIF private key and return the corresponding public key
 * @param wif - Private key in WIF format
 * @param prefix - Public key prefix (default "STM")
 * @return JSON result: {"public_key": "STM..."} or {"error": "..."}
 */
std::string wif_to_public_key(const std::string& wif, const std::string& prefix) {
    auto privkey = parse_wif(wif);
    if (!privkey) {
        return R"({"error": "Invalid WIF format"})";
    }

    auto pubkey = get_public_key(privkey->data());
    if (!pubkey) {
        return R"({"error": "Failed to derive public key"})";
    }

    std::string result = format_public_key(*pubkey, prefix);
    return R"({"public_key": ")" + result + R"("})";
}

/**
 * Validate WIF format
 * @param wif - Private key in WIF format
 * @return JSON result: {"valid": true/false}
 */
std::string validate_wif(const std::string& wif) {
    auto privkey = parse_wif(wif);
    return privkey ? R"({"valid": true})" : R"({"valid": false})";
}

/**
 * Sign a digest with a private key
 * @param wif - Private key in WIF format
 * @param digest_hex - 32-byte digest in hex format
 * @return JSON result: {"signature": "..."} or {"error": "..."}
 */
std::string sign_digest(const std::string& wif, const std::string& digest_hex) {
    auto privkey = parse_wif(wif);
    if (!privkey) {
        return R"({"error": "Invalid WIF format"})";
    }

    auto digest = hex_to_bytes(digest_hex);
    if (digest.size() != 32) {
        return R"({"error": "Digest must be 32 bytes"})";
    }

    auto ctx = get_context();
    secp256k1_ecdsa_recoverable_signature sig;

    if (!secp256k1_ecdsa_sign_recoverable(ctx, &sig, digest.data(), privkey->data(),
                                           secp256k1_nonce_function_rfc6979, nullptr)) {
        return R"({"error": "Signing failed"})";
    }

    // Serialize to compact format (64 bytes) + recovery id
    std::array<uint8_t, 65> serialized;
    int recid;

    if (!secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, serialized.data() + 1,
                                                                   &recid, &sig)) {
        return R"({"error": "Signature serialization failed"})";
    }

    // Hive uses compact signature format: recovery_id + 27 + 4 (compressed)
    serialized[0] = static_cast<uint8_t>(27 + 4 + recid);

    return R"({"signature": ")" + bytes_to_hex(serialized.data(), serialized.size()) + R"("})";
}

/**
 * Compute ECDH shared secret (for ECIES encryption)
 * @param wif - Private key in WIF format
 * @param pubkey_str - Public key with prefix
 * @param prefix - Public key prefix (default "STM")
 * @return JSON result: {"shared_secret": "..."} (64 hex chars) or {"error": "..."}
 */
std::string get_shared_secret(const std::string& wif, const std::string& pubkey_str, const std::string& prefix) {
    auto privkey = parse_wif(wif);
    if (!privkey) {
        return R"({"error": "Invalid WIF format"})";
    }

    auto pubkey_bytes = parse_public_key(pubkey_str, prefix);
    if (!pubkey_bytes) {
        return R"({"error": "Invalid public key format"})";
    }

    auto ctx = get_context();
    secp256k1_pubkey pubkey;

    if (!secp256k1_ec_pubkey_parse(ctx, &pubkey, pubkey_bytes->data(), pubkey_bytes->size())) {
        return R"({"error": "Failed to parse public key"})";
    }

    // Multiply public key by private key (ECDH)
    if (!secp256k1_ec_pubkey_tweak_mul(ctx, &pubkey, privkey->data())) {
        return R"({"error": "ECDH computation failed"})";
    }

    // Serialize the result
    std::array<uint8_t, 33> shared_point;
    size_t len = shared_point.size();
    secp256k1_ec_pubkey_serialize(ctx, shared_point.data(), &len, &pubkey, SECP256K1_EC_COMPRESSED);

    // Hash the x-coordinate (skip the prefix byte)
    // Note: In FC, they hash bytes 1-32 and return 64 bytes via SHA512
    // Here we return 32 bytes (SHA256 of x-coordinate) for JS to process further
    auto hash = minimal::SHA256::hash(shared_point.data() + 1, 32);

    return R"({"shared_secret": ")" + bytes_to_hex(hash.data(), hash.size()) + R"("})";
}

/**
 * Get raw private key bytes from WIF (for internal use)
 * @param wif - Private key in WIF format
 * @return JSON result: {"private_key": "..."} (64 hex chars) or {"error": "..."}
 */
std::string get_private_key_bytes(const std::string& wif) {
    auto privkey = parse_wif(wif);
    if (!privkey) {
        return R"({"error": "Invalid WIF format"})";
    }

    return R"({"private_key": ")" + bytes_to_hex(privkey->data(), privkey->size()) + R"("})";
}

/**
 * Debug: decode base58 string and return info
 */
std::string debug_base58(const std::string& input) {
    auto decoded = minimal::from_base58(input);
    std::string result = R"({"input_length": )" + std::to_string(input.length()) +
                         R"(, "decoded_length": )" + std::to_string(decoded.size());

    if (!decoded.empty()) {
        result += R"(, "first_byte": "0x)" + bytes_to_hex(&decoded[0], 1) + R"(")";
        result += R"(, "all_bytes": ")" + bytes_to_hex(decoded.data(), decoded.size()) + R"(")";

        // Test checksum
        if (decoded.size() >= 5) {
            size_t data_len = decoded.size() - 4;
            auto computed_checksum = minimal::double_sha256(decoded.data(), data_len);
            result += R"(, "computed_checksum": ")" + bytes_to_hex(computed_checksum.data(), 4) + R"(")";
            result += R"(, "stored_checksum": ")" + bytes_to_hex(decoded.data() + data_len, 4) + R"(")";
        }
    }

    result += "}";
    return result;
}

// Emscripten bindings
EMSCRIPTEN_BINDINGS(beekeeper_minimal) {
    emscripten::function("wif_to_public_key", &wif_to_public_key);
    emscripten::function("validate_wif", &validate_wif);
    emscripten::function("sign_digest", &sign_digest);
    emscripten::function("get_shared_secret", &get_shared_secret);
    emscripten::function("get_private_key_bytes", &get_private_key_bytes);
    emscripten::function("debug_base58", &debug_base58);
}
