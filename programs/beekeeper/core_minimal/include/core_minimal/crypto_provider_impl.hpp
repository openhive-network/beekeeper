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
  /// PBKDF2-HMAC-SHA512 work factor written into newly encrypted wallets.
  /// ~0.2-0.5 s on current hardware; stored in the wallet header, so existing
  /// wallets keep the parameters they were encrypted with.
  static constexpr uint32_t default_kdf_iterations = 600'000;

  /// Upper bound accepted when parsing a wallet header. Protects against a
  /// corrupted/hostile file turning unlock into a multi-minute stall.
  static constexpr uint32_t max_kdf_iterations = 10'000'000;

  /// @param prims           Low-level crypto primitives (ownership NOT taken).
  /// @param kdf_iterations  PBKDF2 work factor for newly encrypted wallets.
  explicit crypto_provider_impl(crypto_primitives& prims,
                                uint32_t kdf_iterations = default_kdf_iterations);

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
  wallet_key derive_wallet_key(
      const std::string& password, uint32_t iterations) override;
  std::vector<char> encrypt_wallet_keys(
      const wallet_key& key, const keys_map& keys) override;
  std::vector<char> encrypt_wallet_data(
      const std::string& password, const keys_map& keys) override;
  keys_map decrypt_wallet_data(
      const std::string& password, const std::vector<char>& cipher_keys) override;
  keys_map decrypt_wallet_data(
      const std::string& password, const std::vector<char>& cipher_keys,
      wallet_key& key) override;
  std::vector<char> parse_wallet_file(
      const std::vector<char>& wallet_file_content) override;
  void validate_password(
      const std::string& password, const std::vector<char>& cipher_keys) override;
  bool is_legacy_wallet(const std::vector<char>& cipher_keys) const override;

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

  /// Header fields of a current-format (v1) wallet blob.
  struct wallet_header_v1
  {
    uint32_t                iterations = 0;
    std::vector<uint8_t>    salt;
    std::array<uint8_t, 16> iv{};
    size_t                  cipher_offset = 0;  ///< position of the ciphertext in the blob
    size_t                  cipher_len = 0;
  };

  /// Current wallet format: salted PBKDF2-HMAC-SHA512 + AES-256-CBC + HMAC-SHA256 tag.
  /// Header sanity checks happen here, before any KDF work.
  wallet_header_v1 parse_wallet_header_v1(const std::vector<char>& cipher_keys) const;

  /// PBKDF2 of the password into key.dk, using key.salt and key.iterations.
  void derive_wallet_key_material(const std::string& password, wallet_key& key);

  /// Derive the key from the header parameters and check the HMAC tag; the
  /// tag doubles as the password check. Throws "Invalid password" on mismatch.
  void verify_wallet_mac_v1(const std::string& password, const std::vector<char>& cipher_keys,
                            const wallet_header_v1& header, wallet_key& key);

  keys_map decrypt_wallet_data_v1(const std::vector<char>& cipher_keys,
                                  const wallet_header_v1& header, const wallet_key& key);

  /// Legacy format: unsalted SHA-512 of the password as AES key+IV, no MAC.
  /// Kept for reading wallets created before the v1 format; never written.
  keys_map decrypt_wallet_data_legacy(const std::string& password, const std::vector<char>& cipher_keys);

  uint32_t kdf_iterations_;
};

} // namespace beekeeper_minimal
