#include <fc_crypto_bridge/fc_crypto_provider.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/sha512.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/crypto/rand.hpp>

#include <cstring>
#include <stdexcept>

namespace {

// ── Type conversion helpers ────────────────────────────────────

fc::ecc::private_key to_fc_private(const beekeeper_minimal::private_key_type& key)
{
  fc::sha256 secret;
  std::memcpy(secret.data(), key.data.data(), 32);
  return fc::ecc::private_key::regenerate(secret);
}

fc::ecc::public_key to_fc_public(const beekeeper_minimal::public_key_type& key)
{
  fc::ecc::public_key_data pkd;
  std::memcpy(pkd.data, key.data.data(), 33);
  return fc::ecc::public_key(pkd);
}

} // anonymous namespace

namespace beekeeper_minimal {

// ── fc_crypto_provider (just wires primitives → impl) ──────────

fc_crypto_provider::fc_crypto_provider()
  : crypto_provider_impl(prims_)
{
}

// ── Hashing ────────────────────────────────────────────────────

digest_type fc_crypto_primitives::sha256(const uint8_t* data, size_t len)
{
  auto h = fc::sha256::hash(reinterpret_cast<const char*>(data),
                             static_cast<uint32_t>(len));
  digest_type result;
  std::memcpy(result.data.data(), h.data(), 32);
  return result;
}

sha512_hash fc_crypto_primitives::sha512(const uint8_t* data, size_t len)
{
  auto h = fc::sha512::hash(reinterpret_cast<const char*>(data),
                             static_cast<uint32_t>(len));
  sha512_hash result;
  std::memcpy(result.data.data(), h.data(), 64);
  return result;
}

std::array<uint8_t, 20> fc_crypto_primitives::ripemd160(const uint8_t* data, size_t len)
{
  auto h = fc::ripemd160::hash(reinterpret_cast<const char*>(data),
                                static_cast<uint32_t>(len));
  std::array<uint8_t, 20> result;
  std::memcpy(result.data(), h.data(), 20);
  return result;
}

// ── AES ────────────────────────────────────────────────────────

std::vector<uint8_t> fc_crypto_primitives::aes256_cbc_encrypt(
    const uint8_t* key, const uint8_t* iv,
    const uint8_t* data, size_t len)
{
  // FC's aes_encrypt takes a sha512 as key (first 32 = key, next 16 = IV)
  // We reconstruct the sha512 from our separate key + iv
  fc::sha512 combined;
  std::memset(combined.data(), 0, 64);
  std::memcpy(combined.data(), key, 32);
  std::memcpy(reinterpret_cast<char*>(combined.data()) + 32, iv, 16);

  std::vector<char> plain(reinterpret_cast<const char*>(data),
                           reinterpret_cast<const char*>(data) + len);
  auto encrypted = fc::aes_encrypt(combined, plain);

  return std::vector<uint8_t>(
    reinterpret_cast<const uint8_t*>(encrypted.data()),
    reinterpret_cast<const uint8_t*>(encrypted.data()) + encrypted.size());
}

std::vector<uint8_t> fc_crypto_primitives::aes256_cbc_decrypt(
    const uint8_t* key, const uint8_t* iv,
    const uint8_t* data, size_t len)
{
  fc::sha512 combined;
  std::memset(combined.data(), 0, 64);
  std::memcpy(combined.data(), key, 32);
  std::memcpy(reinterpret_cast<char*>(combined.data()) + 32, iv, 16);

  std::vector<char> cipher(reinterpret_cast<const char*>(data),
                            reinterpret_cast<const char*>(data) + len);
  auto decrypted = fc::aes_decrypt(combined, cipher);

  return std::vector<uint8_t>(
    reinterpret_cast<const uint8_t*>(decrypted.data()),
    reinterpret_cast<const uint8_t*>(decrypted.data()) + decrypted.size());
}

// ── secp256k1 ──────────────────────────────────────────────────

private_key_type fc_crypto_primitives::generate_private_key()
{
  auto fc_key = fc::ecc::private_key::generate();
  private_key_type result;
  auto secret = fc_key.get_secret();
  std::memcpy(result.data.data(), secret.data(), 32);
  return result;
}

public_key_type fc_crypto_primitives::get_public_key(const private_key_type& privkey)
{
  auto fc_key = to_fc_private(privkey);
  auto fc_pub = fc_key.get_public_key();
  auto serialized = fc_pub.serialize();

  public_key_type result;
  std::memcpy(result.data.data(), serialized.data, 33);
  return result;
}

signature_type fc_crypto_primitives::sign_compact(const private_key_type& privkey,
                                                   const digest_type& digest)
{
  auto fc_key = to_fc_private(privkey);
  fc::sha256 fc_digest;
  std::memcpy(fc_digest.data(), digest.data.data(), 32);

  auto fc_sig = fc_key.sign_compact(fc_digest);

  signature_type result;
  std::memcpy(result.data.data(), fc_sig.data, 65);
  return result;
}

sha512_hash fc_crypto_primitives::ecdh_shared_secret(const private_key_type& privkey,
                                                      const public_key_type& pubkey)
{
  auto fc_priv = to_fc_private(privkey);
  auto fc_pub = to_fc_public(pubkey);
  auto shared = fc_priv.get_shared_secret(fc_pub);

  sha512_hash result;
  std::memcpy(result.data.data(), shared.data(), 64);
  return result;
}

// ── Base58 ─────────────────────────────────────────────────────

std::string fc_crypto_primitives::base58_encode(const uint8_t* data, size_t len)
{
  return fc::to_base58(reinterpret_cast<const char*>(data),
                        static_cast<size_t>(len));
}

std::vector<uint8_t> fc_crypto_primitives::base58_decode(const std::string& str)
{
  auto decoded = fc::from_base58(str);
  return std::vector<uint8_t>(decoded.begin(), decoded.end());
}

// ── Random ──────────────────────────────────────────────────

void fc_crypto_primitives::get_random_bytes(uint8_t* buf, size_t len)
{
  fc::rand_bytes(reinterpret_cast<char*>(buf), static_cast<int>(len));
}

} // namespace beekeeper_minimal
