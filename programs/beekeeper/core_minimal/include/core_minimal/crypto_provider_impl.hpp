#pragma once

/// Base class implementing all crypto_provider methods using crypto_primitives.
/// Both fc_crypto_provider and js_crypto_provider inherit from this,
/// so the high-level wallet encryption logic is shared (single implementation).

#include <core_minimal/crypto_provider.hpp>
#include <core_minimal/crypto_primitives.hpp>

namespace beekeeper_minimal {

class crypto_provider_impl : public crypto_provider
{
public:
  /// @param prims  Low-level crypto primitives (ownership NOT taken).
  explicit crypto_provider_impl(crypto_primitives& prims);

  // ── crypto_provider interface (all implemented) ──────────────

  private_key_type generate_private_key() override;
  std::optional<private_key_type> wif_to_key(const std::string& wif) override;
  std::string key_to_wif(const private_key_type& key) override;
  public_key_type get_public_key(const private_key_type& key) override;
  std::string public_key_to_string(const public_key_type& key,
                                   const std::string& prefix) override;
  public_key_type public_key_from_string(const std::string& str,
                                         const std::string& prefix) override;
  signature_type sign_compact(const private_key_type& key,
                               const digest_type& digest) override;
  digest_type digest_from_hex(const std::string& hex) override;
  std::string signature_to_hex(const signature_type& sig) override;

  std::vector<char> encrypt_wallet_keys(
      const std::string& password, const keys_map& keys) override;
  std::vector<char> encrypt_wallet_data(
      const std::string& password, const keys_map& keys) override;
  keys_map decrypt_wallet_data(
      const std::string& password, const std::vector<char>& cipher_keys) override;
  std::vector<char> parse_wallet_file(
      const std::vector<char>& wallet_file_content) override;
  void validate_password(
      const std::string& password, const std::vector<char>& cipher_keys) override;

  std::string ecdh_encrypt(
      const private_key_type& from_key, const public_key_type& to_key,
      const std::string& content, std::optional<uint64_t> nonce = {}) override;
  std::string ecdh_decrypt(
      const private_key_type& priv_key, const public_key_type& other_pub,
      const std::string& encrypted_content) override;

  void get_random_bytes(uint8_t* buf, size_t len) override;

protected:
  crypto_primitives& prims_;

private:
  /// AES-encrypt using a sha512_hash as combined key+IV.
  /// Key = hash[0:32], IV = hash[32:48].
  std::vector<char> aes_encrypt(const sha512_hash& hash, const std::vector<char>& data);
  std::vector<char> aes_decrypt(const sha512_hash& hash, const std::vector<char>& data);

  /// SHA512(pack(nonce) || shared_secret) — matches FC's generate_encrypted_key
  sha512_hash generate_encrypted_key(uint64_t nonce, const sha512_hash& shared_secret);
};

} // namespace beekeeper_minimal
