// Base58 encoding/decoding without OpenSSL dependencies
// Based on Bitcoin's implementation, simplified for minimal footprint

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <array>

namespace minimal {

// Base58 alphabet (Bitcoin style)
constexpr const char* BASE58_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// Decode base58 alphabet character to value (0-57), or -1 if invalid
inline int base58_char_value(char c) {
    if (c >= '1' && c <= '9') return c - '1';
    if (c >= 'A' && c <= 'H') return c - 'A' + 9;
    if (c >= 'J' && c <= 'N') return c - 'J' + 17;
    if (c >= 'P' && c <= 'Z') return c - 'P' + 22;
    if (c >= 'a' && c <= 'k') return c - 'a' + 33;
    if (c >= 'm' && c <= 'z') return c - 'm' + 44;
    return -1;
}

// Encode bytes to base58 string
inline std::string to_base58(const uint8_t* data, size_t size) {
    // Count leading zeros
    size_t leading_zeros = 0;
    while (leading_zeros < size && data[leading_zeros] == 0) {
        ++leading_zeros;
    }

    // Allocate enough space for result (worst case: size * 138 / 100 + 1)
    std::vector<uint8_t> b58((size - leading_zeros) * 138 / 100 + 1, 0);

    // Process input bytes
    for (size_t i = leading_zeros; i < size; ++i) {
        int carry = data[i];
        for (auto it = b58.rbegin(); it != b58.rend(); ++it) {
            carry += 256 * (*it);
            *it = carry % 58;
            carry /= 58;
        }
    }

    // Skip leading zeros in b58 result
    auto it = b58.begin();
    while (it != b58.end() && *it == 0) {
        ++it;
    }

    // Build result string
    std::string result;
    result.reserve(leading_zeros + (b58.end() - it));
    result.assign(leading_zeros, '1');
    while (it != b58.end()) {
        result += BASE58_ALPHABET[*it++];
    }

    return result;
}

// Decode base58 string to bytes
inline std::vector<uint8_t> from_base58(const std::string& str) {
    // Count leading '1's (zeros)
    size_t leading_zeros = 0;
    while (leading_zeros < str.size() && str[leading_zeros] == '1') {
        ++leading_zeros;
    }

    // Allocate enough space for result
    std::vector<uint8_t> result((str.size() - leading_zeros) * 733 / 1000 + 1, 0);

    // Process input characters
    for (size_t i = leading_zeros; i < str.size(); ++i) {
        int value = base58_char_value(str[i]);
        if (value < 0) {
            return {}; // Invalid character
        }

        int carry = value;
        for (auto it = result.rbegin(); it != result.rend(); ++it) {
            carry += 58 * (*it);
            *it = carry % 256;
            carry /= 256;
        }
    }

    // Skip leading zeros in result
    auto it = result.begin();
    while (it != result.end() && *it == 0) {
        ++it;
    }

    // Build final result with leading zeros
    std::vector<uint8_t> final_result;
    final_result.reserve(leading_zeros + (result.end() - it));
    final_result.insert(final_result.end(), leading_zeros, 0);
    final_result.insert(final_result.end(), it, result.end());

    return final_result;
}

// Simple SHA-256 implementation for checksum verification
// This is a minimal implementation - in production, consider using a proper library
class SHA256 {
public:
    static constexpr size_t DIGEST_SIZE = 32;

    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            m_data[m_blocklen++] = data[i];
            if (m_blocklen == 64) {
                transform();
                m_bitlen += 512;
                m_blocklen = 0;
            }
        }
    }

    std::array<uint8_t, DIGEST_SIZE> finalize() {
        std::array<uint8_t, DIGEST_SIZE> hash;
        pad();
        revert(hash.data());
        return hash;
    }

    static std::array<uint8_t, DIGEST_SIZE> hash(const uint8_t* data, size_t len) {
        SHA256 sha;
        sha.update(data, len);
        return sha.finalize();
    }

private:
    uint8_t m_data[64] = {};
    uint32_t m_blocklen = 0;
    uint64_t m_bitlen = 0;
    uint32_t m_state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    static constexpr uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static uint32_t sig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
    static uint32_t sig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
    static uint32_t ep0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
    static uint32_t ep1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

    void transform() {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (m_data[i * 4] << 24) | (m_data[i * 4 + 1] << 16) |
                   (m_data[i * 4 + 2] << 8) | m_data[i * 4 + 3];
        }
        for (int i = 16; i < 64; ++i) {
            w[i] = ep1(w[i - 2]) + w[i - 7] + ep0(w[i - 15]) + w[i - 16];
        }

        uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
        uint32_t e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = h + sig1(e) + ch(e, f, g) + K[i] + w[i];
            uint32_t t2 = sig0(a) + maj(a, b, c);
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }

        m_state[0] += a; m_state[1] += b; m_state[2] += c; m_state[3] += d;
        m_state[4] += e; m_state[5] += f; m_state[6] += g; m_state[7] += h;
    }

    void pad() {
        uint64_t i = m_blocklen;
        if (m_blocklen < 56) {
            m_data[i++] = 0x80;
            while (i < 56) m_data[i++] = 0x00;
        } else {
            m_data[i++] = 0x80;
            while (i < 64) m_data[i++] = 0x00;
            transform();
            std::fill(m_data, m_data + 56, 0);
        }
        m_bitlen += m_blocklen * 8;
        for (int j = 7; j >= 0; --j) {
            m_data[56 + (7 - j)] = m_bitlen >> (j * 8);
        }
        transform();
    }

    void revert(uint8_t* hash) {
        for (int i = 0; i < 8; ++i) {
            hash[i * 4] = (m_state[i] >> 24) & 0xff;
            hash[i * 4 + 1] = (m_state[i] >> 16) & 0xff;
            hash[i * 4 + 2] = (m_state[i] >> 8) & 0xff;
            hash[i * 4 + 3] = m_state[i] & 0xff;
        }
    }
};

constexpr uint32_t SHA256::K[64];

// Double SHA-256 (used for WIF checksum)
inline std::array<uint8_t, 32> double_sha256(const uint8_t* data, size_t len) {
    auto first = SHA256::hash(data, len);
    return SHA256::hash(first.data(), first.size());
}

// RIPEMD-160 implementation for public key checksums (Hive format)
class RIPEMD160 {
public:
    static constexpr size_t DIGEST_SIZE = 20;

    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            m_data[m_blocklen++] = data[i];
            if (m_blocklen == 64) {
                transform();
                m_bitlen += 512;
                m_blocklen = 0;
            }
        }
    }

    std::array<uint8_t, DIGEST_SIZE> finalize() {
        std::array<uint8_t, DIGEST_SIZE> hash;
        pad();
        revert(hash.data());
        return hash;
    }

    static std::array<uint8_t, DIGEST_SIZE> hash(const uint8_t* data, size_t len) {
        RIPEMD160 rmd;
        rmd.update(data, len);
        return rmd.finalize();
    }

private:
    uint8_t m_data[64] = {};
    uint32_t m_blocklen = 0;
    uint64_t m_bitlen = 0;
    uint32_t m_state[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };

    static uint32_t rotl(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }
    static uint32_t f(uint32_t j, uint32_t x, uint32_t y, uint32_t z) {
        if (j < 16) return x ^ y ^ z;
        if (j < 32) return (x & y) | (~x & z);
        if (j < 48) return (x | ~y) ^ z;
        if (j < 64) return (x & z) | (y & ~z);
        return x ^ (y | ~z);
    }
    static uint32_t K(uint32_t j) {
        if (j < 16) return 0x00000000;
        if (j < 32) return 0x5a827999;
        if (j < 48) return 0x6ed9eba1;
        if (j < 64) return 0x8f1bbcdc;
        return 0xa953fd4e;
    }
    static uint32_t Kp(uint32_t j) {
        if (j < 16) return 0x50a28be6;
        if (j < 32) return 0x5c4dd124;
        if (j < 48) return 0x6d703ef3;
        if (j < 64) return 0x7a6d76e9;
        return 0x00000000;
    }
    static constexpr uint8_t r[80] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        7, 4, 13, 1, 10, 6, 15, 3, 12, 0, 9, 5, 2, 14, 11, 8,
        3, 10, 14, 4, 9, 15, 8, 1, 2, 7, 0, 6, 13, 11, 5, 12,
        1, 9, 11, 10, 0, 8, 12, 4, 13, 3, 7, 15, 14, 5, 6, 2,
        4, 0, 5, 9, 7, 12, 2, 10, 14, 1, 3, 8, 11, 6, 15, 13
    };
    static constexpr uint8_t rp[80] = {
        5, 14, 7, 0, 9, 2, 11, 4, 13, 6, 15, 8, 1, 10, 3, 12,
        6, 11, 3, 7, 0, 13, 5, 10, 14, 15, 8, 12, 4, 9, 1, 2,
        15, 5, 1, 3, 7, 14, 6, 9, 11, 8, 12, 2, 10, 0, 4, 13,
        8, 6, 4, 1, 3, 11, 15, 0, 5, 12, 2, 13, 9, 7, 10, 14,
        12, 15, 10, 4, 1, 5, 8, 7, 6, 2, 13, 14, 0, 3, 9, 11
    };
    static constexpr uint8_t s[80] = {
        11, 14, 15, 12, 5, 8, 7, 9, 11, 13, 14, 15, 6, 7, 9, 8,
        7, 6, 8, 13, 11, 9, 7, 15, 7, 12, 15, 9, 11, 7, 13, 12,
        11, 13, 6, 7, 14, 9, 13, 15, 14, 8, 13, 6, 5, 12, 7, 5,
        11, 12, 14, 15, 14, 15, 9, 8, 9, 14, 5, 6, 8, 6, 5, 12,
        9, 15, 5, 11, 6, 8, 13, 12, 5, 12, 13, 14, 11, 8, 5, 6
    };
    static constexpr uint8_t sp[80] = {
        8, 9, 9, 11, 13, 15, 15, 5, 7, 7, 8, 11, 14, 14, 12, 6,
        9, 13, 15, 7, 12, 8, 9, 11, 7, 7, 12, 7, 6, 15, 13, 11,
        9, 7, 15, 11, 8, 6, 6, 14, 12, 13, 5, 14, 13, 13, 7, 5,
        15, 5, 8, 11, 14, 14, 6, 14, 6, 9, 12, 9, 12, 5, 15, 8,
        8, 5, 12, 9, 12, 5, 14, 6, 8, 13, 6, 5, 15, 13, 11, 11
    };

    void transform() {
        uint32_t X[16];
        for (int i = 0; i < 16; ++i) {
            X[i] = m_data[i * 4] | (m_data[i * 4 + 1] << 8) |
                   (m_data[i * 4 + 2] << 16) | (m_data[i * 4 + 3] << 24);
        }

        uint32_t A = m_state[0], B = m_state[1], C = m_state[2], D = m_state[3], E = m_state[4];
        uint32_t Ap = m_state[0], Bp = m_state[1], Cp = m_state[2], Dp = m_state[3], Ep = m_state[4];

        for (uint32_t j = 0; j < 80; ++j) {
            uint32_t T = rotl(A + f(j, B, C, D) + X[r[j]] + K(j), s[j]) + E;
            A = E; E = D; D = rotl(C, 10); C = B; B = T;

            T = rotl(Ap + f(79 - j, Bp, Cp, Dp) + X[rp[j]] + Kp(j), sp[j]) + Ep;
            Ap = Ep; Ep = Dp; Dp = rotl(Cp, 10); Cp = Bp; Bp = T;
        }

        uint32_t T = m_state[1] + C + Dp;
        m_state[1] = m_state[2] + D + Ep;
        m_state[2] = m_state[3] + E + Ap;
        m_state[3] = m_state[4] + A + Bp;
        m_state[4] = m_state[0] + B + Cp;
        m_state[0] = T;
    }

    void pad() {
        uint64_t i = m_blocklen;
        m_data[i++] = 0x80;
        if (m_blocklen < 56) {
            while (i < 56) m_data[i++] = 0x00;
        } else {
            while (i < 64) m_data[i++] = 0x00;
            transform();
            std::fill(m_data, m_data + 56, 0);
        }
        m_bitlen += m_blocklen * 8;
        // Little-endian length
        for (int j = 0; j < 8; ++j) {
            m_data[56 + j] = m_bitlen >> (j * 8);
        }
        transform();
    }

    void revert(uint8_t* hash) {
        for (int i = 0; i < 5; ++i) {
            hash[i * 4] = m_state[i] & 0xff;
            hash[i * 4 + 1] = (m_state[i] >> 8) & 0xff;
            hash[i * 4 + 2] = (m_state[i] >> 16) & 0xff;
            hash[i * 4 + 3] = (m_state[i] >> 24) & 0xff;
        }
    }
};

constexpr uint8_t RIPEMD160::r[80];
constexpr uint8_t RIPEMD160::rp[80];
constexpr uint8_t RIPEMD160::s[80];
constexpr uint8_t RIPEMD160::sp[80];

// RIPEMD160 hash helper
inline std::array<uint8_t, 20> ripemd160(const uint8_t* data, size_t len) {
    return RIPEMD160::hash(data, len);
}

} // namespace minimal
