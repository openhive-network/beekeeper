#pragma once

/// Standalone Base58 encoding/decoding — no OpenSSL, no bignum library.
/// Uses Bitcoin's standard alphabet: 123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace beekeeper_wasm {

std::string standalone_base58_encode(const uint8_t* data, size_t len);
std::vector<uint8_t> standalone_base58_decode(const std::string& str);

} // namespace beekeeper_wasm
