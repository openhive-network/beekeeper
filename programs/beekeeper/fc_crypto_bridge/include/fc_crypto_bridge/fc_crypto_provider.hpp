#pragma once

#include <core_minimal/crypto_provider_impl.hpp>
#include <core_minimal/crypto_primitives.hpp>

namespace beekeeper_minimal {

/// Low-level crypto primitives backed by the FC library (OpenSSL / secp256k1).
/// All high-level methods (wallet encryption, ECDH messaging, WIF, etc.) are
/// inherited from crypto_provider_impl.
class fc_crypto_primitives final : public crypto_primitives
{
public:
  digest_type sha256(const uint8_t* data, size_t len) override;
  sha512_hash sha512(const uint8_t* data, size_t len) override;
  std::array<uint8_t, 20> ripemd160(const uint8_t* data, size_t len) override;

  std::vector<uint8_t> aes256_cbc_encrypt(
      const uint8_t* key, const uint8_t* iv,
      const uint8_t* data, size_t len) override;
  std::vector<uint8_t> aes256_cbc_decrypt(
      const uint8_t* key, const uint8_t* iv,
      const uint8_t* data, size_t len) override;

  private_key_type generate_private_key() override;
  public_key_type get_public_key(const private_key_type& privkey) override;
  signature_type sign_compact(const private_key_type& privkey,
                               const digest_type& digest) override;

  sha512_hash ecdh_shared_secret(const private_key_type& privkey,
                                  const public_key_type& pubkey) override;

  std::string base58_encode(const uint8_t* data, size_t len) override;
  std::vector<uint8_t> base58_decode(const std::string& str) override;

  void get_random_bytes(uint8_t* buf, size_t len) override;
};

/// Full crypto_provider backed by FC.
/// Inherits all high-level logic from crypto_provider_impl,
/// primitives from fc_crypto_primitives.
class fc_crypto_provider final : public crypto_provider_impl
{
public:
  fc_crypto_provider();

private:
  fc_crypto_primitives prims_;
};

} // namespace beekeeper_minimal
