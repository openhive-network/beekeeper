#pragma once

#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/sha512.hpp>
#include <fc/io/raw.hpp>

#include <map>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <functional>
#include <chrono>

namespace beekeeper_minimal {

using private_key_type = fc::ecc::private_key;
using public_key_type  = fc::ecc::public_key;
using signature_type   = fc::ecc::compact_signature;
using digest_type      = fc::sha256;

/// (private_key, public_key_prefix)
using key_data    = std::pair<private_key_type, std::string>;
/// public_key -> (private_key, prefix)
using keys_map    = std::map<public_key_type, key_data>;

/// What gets AES-encrypted inside a wallet file
struct plain_keys
{
  fc::sha512  checksum;
  keys_map    keys;
};

/// Serialized wallet blob (the thing that goes to/from storage)
struct wallet_data
{
  std::vector<char> cipher_keys;
};

/// Convert a public key to its prefixed string form
inline std::string public_key_to_string(const public_key_type& key, const std::string& prefix)
{
  return prefix + public_key_type::to_base58(key, false);
}

/// Parse a prefixed public key string
inline public_key_type public_key_from_string(const std::string& str, const std::string& prefix)
{
  if (str.substr(0, prefix.size()) != prefix)
    throw std::invalid_argument("public key requires prefix: " + prefix);
  return public_key_type::from_base58(str.substr(prefix.size()), false);
}

} // namespace beekeeper_minimal

FC_REFLECT(beekeeper_minimal::plain_keys, (checksum)(keys))
FC_REFLECT(beekeeper_minimal::wallet_data, (cipher_keys))

namespace fc {

/// wallet_data is serialized as just its cipher_keys vector (matching the original beekeeper format)
inline void from_variant(const fc::variant& var, beekeeper_minimal::wallet_data& vo)
{
  from_variant(var, vo.cipher_keys);
}

inline void to_variant(const beekeeper_minimal::wallet_data& var, fc::variant& vo)
{
  to_variant(var.cipher_keys, vo);
}

} // namespace fc
