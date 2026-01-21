#pragma once
// Minimal FC hex conversion for WASM build
// Header-only implementation - no dependencies on OpenSSL or full FC

#include <fc/string.hpp>
#include <vector>
#include <cstdint>

namespace fc {

// Convert single hex character to value (0-15)
// Returns 0 for invalid characters (caller should validate input)
inline uint8_t from_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0; // Invalid character - return 0 (minimal build doesn't throw)
}

// Convert bytes to hex string
inline std::string to_hex(const char* d, uint32_t s) {
    static const char* hex_chars = "0123456789abcdef";
    std::string r;
    r.reserve(s * 2);
    const uint8_t* c = reinterpret_cast<const uint8_t*>(d);
    for (uint32_t i = 0; i < s; ++i) {
        r += hex_chars[(c[i] >> 4) & 0x0f];
        r += hex_chars[c[i] & 0x0f];
    }
    return r;
}

// Convert vector to hex string
inline std::string to_hex(const std::vector<char>& data) {
    if (data.empty()) return "";
    return to_hex(data.data(), static_cast<uint32_t>(data.size()));
}

// Convert hex string to bytes
// Returns number of bytes written
inline size_t from_hex(const fc::string& hex_str, char* out_data, size_t out_data_len) {
    uint8_t* out_pos = reinterpret_cast<uint8_t*>(out_data);
    uint8_t* out_end = out_pos + out_data_len;
    auto it = hex_str.begin();

    while (it != hex_str.end() && out_pos != out_end) {
        *out_pos = from_hex(*it) << 4;
        ++it;
        if (it != hex_str.end()) {
            *out_pos |= from_hex(*it);
            ++it;
        }
        ++out_pos;
    }
    return out_pos - reinterpret_cast<uint8_t*>(out_data);
}

// Additional convenience functions for minimal WASM API

// Convert bytes to hex string (uint8_t variant)
inline std::string to_hex(const uint8_t* data, size_t len) {
    return to_hex(reinterpret_cast<const char*>(data), static_cast<uint32_t>(len));
}

// Convert hex string to vector of bytes
inline std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    if (hex.length() % 2 != 0) return {};

    std::vector<uint8_t> bytes;
    bytes.reserve(hex.length() / 2);

    for (size_t i = 0; i < hex.length(); i += 2) {
        char c1 = hex[i];
        char c2 = hex[i + 1];

        // Validate hex characters
        auto is_hex = [](char c) {
            return (c >= '0' && c <= '9') ||
                   (c >= 'a' && c <= 'f') ||
                   (c >= 'A' && c <= 'F');
        };

        if (!is_hex(c1) || !is_hex(c2)) return {};

        bytes.push_back((from_hex(c1) << 4) | from_hex(c2));
    }
    return bytes;
}

} // namespace fc
