#pragma once

#include <core_minimal/crypto_types.hpp>

#include <vector>

namespace beekeeper_minimal {

// All crypto types (digest_type, public_key_type, private_key_type,
// signature_type, key_data, keys_map) come from crypto_types.hpp.

/// Serialized wallet blob (the thing that goes to/from storage)
struct wallet_data
{
  std::vector<char> cipher_keys;
};

} // namespace beekeeper_minimal
