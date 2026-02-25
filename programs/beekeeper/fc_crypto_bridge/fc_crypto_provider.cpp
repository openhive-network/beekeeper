#include <fc_crypto_bridge/fc_crypto_provider.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/sha512.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/crypto/crypto_data.hpp>
#include <fc/io/json.hpp>
#include <fc/io/raw.hpp>

#include <cstring>
#include <stdexcept>

namespace {

// ── Type conversion helpers (our types <-> FC types) ──────────

fc::ecc::private_key to_fc_private(const beekeeper_minimal::private_key_type& key)
{
  fc::sha256 secret;
  std::memcpy(secret.data(), key.data.data(), 32);
  return fc::ecc::private_key::regenerate(secret);
}

beekeeper_minimal::private_key_type from_fc_private(const fc::ecc::private_key& key)
{
  beekeeper_minimal::private_key_type result;
  auto secret = key.get_secret();
  std::memcpy(result.data.data(), secret.data(), 32);
  return result;
}

fc::ecc::public_key to_fc_public(const beekeeper_minimal::public_key_type& key)
{
  fc::ecc::public_key_data pkd;
  std::memcpy(pkd.data, key.data.data(), 33);
  return fc::ecc::public_key(pkd);
}

beekeeper_minimal::public_key_type from_fc_public(const fc::ecc::public_key& key)
{
  beekeeper_minimal::public_key_type result;
  auto serialized = key.serialize();
  std::memcpy(result.data.data(), serialized.data, 33);
  return result;
}

fc::sha256 to_fc_digest(const beekeeper_minimal::digest_type& d)
{
  fc::sha256 result;
  std::memcpy(result.data(), d.data.data(), 32);
  return result;
}

beekeeper_minimal::signature_type from_fc_signature(const fc::ecc::compact_signature& sig)
{
  beekeeper_minimal::signature_type result;
  std::memcpy(result.data.data(), sig.data, 65);
  return result;
}

// ── FC serialization types (internal only) ───────────────────
// These mirror the original plain_keys/wallet_data structs but with FC types.
// They exist ONLY inside this bridge for serialization purposes.

using fc_private_key_type = fc::ecc::private_key;
using fc_public_key_type  = fc::ecc::public_key;
using fc_key_data    = std::pair<fc_private_key_type, std::string>;
using fc_keys_map    = std::map<fc_public_key_type, fc_key_data>;

struct fc_plain_keys
{
  fc::sha512    checksum;
  fc_keys_map   keys;
};

struct fc_wallet_data
{
  std::vector<char> cipher_keys;
};

fc_keys_map to_fc_keys(const beekeeper_minimal::keys_map& keys)
{
  fc_keys_map result;
  for (auto& [pub, data] : keys)
    result.emplace(to_fc_public(pub), fc_key_data(to_fc_private(data.first), data.second));
  return result;
}

beekeeper_minimal::keys_map from_fc_keys(const fc_keys_map& keys)
{
  beekeeper_minimal::keys_map result;
  for (auto& [pub, data] : keys)
    result.emplace(from_fc_public(pub),
                   beekeeper_minimal::key_data(from_fc_private(data.first), data.second));
  return result;
}

} // anonymous namespace

FC_REFLECT(fc_plain_keys, (checksum)(keys))
FC_REFLECT(fc_wallet_data, (cipher_keys))

namespace fc {

inline void from_variant(const fc::variant& var, fc_wallet_data& vo)
{
  from_variant(var, vo.cipher_keys);
}

inline void to_variant(const fc_wallet_data& var, fc::variant& vo)
{
  to_variant(var.cipher_keys, vo);
}

} // namespace fc

namespace beekeeper_minimal {

// ── Key operations ────────────────────────────────────────────

private_key_type fc_crypto_provider::generate_private_key()
{
  return from_fc_private(fc::ecc::private_key::generate());
}

std::optional<private_key_type> fc_crypto_provider::wif_to_key(const std::string& wif)
{
  auto key = fc::ecc::private_key::wif_to_key(wif);
  if (!key.valid())
    return std::nullopt;
  return from_fc_private(*key);
}

std::string fc_crypto_provider::key_to_wif(const private_key_type& key)
{
  return to_fc_private(key).key_to_wif();
}

public_key_type fc_crypto_provider::get_public_key(const private_key_type& key)
{
  return from_fc_public(to_fc_private(key).get_public_key());
}

std::string fc_crypto_provider::public_key_to_string(const public_key_type& key,
                                                     const std::string& prefix)
{
  auto fc_key = to_fc_public(key);
  return prefix + fc::ecc::public_key::to_base58(fc_key, false);
}

public_key_type fc_crypto_provider::public_key_from_string(const std::string& str,
                                                           const std::string& prefix)
{
  if (str.substr(0, prefix.size()) != prefix)
    throw std::invalid_argument("public key requires prefix: " + prefix);
  return from_fc_public(fc::ecc::public_key::from_base58(str.substr(prefix.size()), false));
}

// ── Signing ───────────────────────────────────────────────────

signature_type fc_crypto_provider::sign_compact(const private_key_type& key,
                                                const digest_type& digest)
{
  auto fc_sig = to_fc_private(key).sign_compact(to_fc_digest(digest));
  return from_fc_signature(fc_sig);
}

// ── Digest / signature hex ────────────────────────────────────

digest_type fc_crypto_provider::digest_from_hex(const std::string& hex)
{
  auto fc_digest = fc::sha256(hex);
  digest_type result;
  std::memcpy(result.data.data(), fc_digest.data(), 32);
  return result;
}

std::string fc_crypto_provider::signature_to_hex(const signature_type& sig)
{
  return fc::to_hex(reinterpret_cast<const char*>(sig.data.data()), sig.size());
}

// ── Wallet encryption ─────────────────────────────────────────

std::vector<char> fc_crypto_provider::encrypt_wallet_data(
    const std::string& password, const keys_map& keys)
{
  auto pw = fc::sha512::hash(password.c_str(), password.size());

  fc_plain_keys pk;
  pk.checksum = pw;
  pk.keys = to_fc_keys(keys);

  auto plain_txt = fc::raw::pack_to_vector(pk);

  fc_wallet_data wd;
  wd.cipher_keys = fc::aes_encrypt(pw, plain_txt);

  std::string json = fc::json::to_pretty_string(wd);
  return std::vector<char>(json.begin(), json.end());
}

keys_map fc_crypto_provider::decrypt_wallet_data(
    const std::string& password, const std::vector<char>& cipher_keys)
{
  auto pw = fc::sha512::hash(password.c_str(), password.size());

  std::vector<char> decrypted;
  try
  {
    decrypted = fc::aes_decrypt(pw, cipher_keys);
  }
  catch (...)
  {
    throw std::runtime_error("Invalid password");
  }

  // Validate checksum
  fc::sha512 stored_checksum;
  fc::raw::unpack_from_vector<fc::sha512>(decrypted, stored_checksum);
  if (pw != stored_checksum)
    throw std::runtime_error("Invalid password");

  fc_plain_keys pk;
  fc::raw::unpack_from_vector<fc_plain_keys>(decrypted, pk, 0, true);

  return from_fc_keys(pk.keys);
}

std::vector<char> fc_crypto_provider::parse_wallet_file(
    const std::vector<char>& wallet_file_content)
{
  std::string json(wallet_file_content.begin(), wallet_file_content.end());
  auto wd = fc::json::from_string(json, fc::json::format_validation_mode::full)
              .as<fc_wallet_data>();
  return wd.cipher_keys;
}

void fc_crypto_provider::validate_password(
    const std::string& password, const std::vector<char>& cipher_keys)
{
  // Attempt decryption; throws on failure
  decrypt_wallet_data(password, cipher_keys);
}

// ── ECDH ──────────────────────────────────────────────────────

std::string fc_crypto_provider::ecdh_encrypt(
    const private_key_type& from_key, const public_key_type& to_key,
    const std::string& content, std::optional<uint64_t> nonce)
{
  fc::crypto_data cd;
  return cd.encrypt(to_fc_private(from_key), to_fc_public(to_key), content, nonce);
}

std::string fc_crypto_provider::ecdh_decrypt(
    key_finder_type key_finder, const public_key_type& from_key,
    const public_key_type& to_key, const std::string& encrypted_content)
{
  fc::crypto_data cd;

  // Wrap our key_finder into an FC-typed key_finder
  fc::crypto_data::key_finder_type fc_finder =
    [&key_finder](const fc::ecc::public_key& pk) -> fc::optional<fc::ecc::private_key> {
      auto our_pub = from_fc_public(pk);
      auto result = key_finder(our_pub);
      if (result)
        return to_fc_private(*result);
      return {};
    };

  return cd.decrypt(fc_finder, to_fc_public(from_key), to_fc_public(to_key), encrypted_content);
}

} // namespace beekeeper_minimal
