#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace beekeeper_minimal {

/// Fixed-size byte buffer with comparison operators (for use as map keys).
template<size_t N>
struct byte_array
{
  std::array<uint8_t, N> data{};  // zero-initialized

  bool operator==(const byte_array& o) const { return data == o.data; }
  bool operator!=(const byte_array& o) const { return data != o.data; }
  bool operator< (const byte_array& o) const { return data <  o.data; }

  const uint8_t* begin() const { return data.data(); }
  const uint8_t* end()   const { return data.data() + N; }
  uint8_t*       begin()       { return data.data(); }
  uint8_t*       end()         { return data.data() + N; }

  static constexpr size_t size() { return N; }

  bool is_zero() const {
    for (auto b : data)
      if (b != 0) return false;
    return true;
  }
};

/// 32-byte digest (replaces fc::sha256)
using digest_type      = byte_array<32>;

/// 33-byte compressed public key (replaces fc::ecc::public_key)
using public_key_type  = byte_array<33>;

/// 32-byte private key secret (replaces fc::ecc::private_key)
using private_key_type = byte_array<32>;

/// 65-byte compact recoverable signature (replaces fc::ecc::compact_signature)
using signature_type   = byte_array<65>;

/// 64-byte hash (replaces fc::sha512)
using sha512_hash      = byte_array<64>;

/// (private_key, public_key_prefix)
using key_data = std::pair<private_key_type, std::string>;

/// public_key -> (private_key, prefix)
using keys_map = std::map<public_key_type, key_data>;

/// Key material derived from a wallet password: the PBKDF2 output and the
/// parameters that produced it. A wallet keeps it while unlocked, so saves
/// re-encrypt with a fresh IV instead of running the KDF again.
struct wallet_key
{
  std::vector<uint8_t>    salt;
  uint32_t                iterations = 0;  ///< 0: no key (legacy wallet blob)
  std::array<uint8_t, 64> dk{};            ///< cipher key [0:32] | MAC key [32:64]

  wallet_key() = default;
  wallet_key(const wallet_key&) = default;
  wallet_key(wallet_key&&) = default;
  wallet_key& operator=(const wallet_key&) = default;
  wallet_key& operator=(wallet_key&&) = default;
  ~wallet_key() { wipe(); }

  /// Best-effort wipe (volatile keeps the compiler from eliding it).
  void wipe()
  {
    auto p = static_cast<volatile uint8_t*>(dk.data());
    for (size_t i = 0; i < dk.size(); ++i)
      p[i] = 0;
    salt.clear();
    iterations = 0;
  }
};

} // namespace beekeeper_minimal
