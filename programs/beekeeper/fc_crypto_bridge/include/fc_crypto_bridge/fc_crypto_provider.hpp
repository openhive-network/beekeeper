#pragma once

#include <core_minimal/crypto_provider.hpp>

namespace beekeeper_minimal {

/// crypto_provider implementation backed by the FC library.
/// Handles all FC type conversions internally.
class fc_crypto_provider final : public crypto_provider
{
public:
  fc_crypto_provider() = default;

  // ── Key operations ──────────────────────────────────────────
  private_key_type generate_private_key() override;
  std::optional<private_key_type> wif_to_key(const std::string& wif) override;
  std::string key_to_wif(const private_key_type& key) override;
  public_key_type get_public_key(const private_key_type& key) override;
  std::string public_key_to_string(const public_key_type& key,
                                   const std::string& prefix) override;
  public_key_type public_key_from_string(const std::string& str,
                                         const std::string& prefix) override;

  // ── Signing ─────────────────────────────────────────────────
  signature_type sign_compact(const private_key_type& key,
                              const digest_type& digest) override;

  // ── Digest / signature hex ──────────────────────────────────
  digest_type digest_from_hex(const std::string& hex) override;
  std::string signature_to_hex(const signature_type& sig) override;

  // ── Wallet encryption ───────────────────────────────────────
  std::vector<char> encrypt_wallet_data(const std::string& password,
                                        const keys_map& keys) override;
  keys_map decrypt_wallet_data(const std::string& password,
                               const std::vector<char>& cipher_keys) override;
  std::vector<char> parse_wallet_file(const std::vector<char>& wallet_file_content) override;
  void validate_password(const std::string& password,
                         const std::vector<char>& cipher_keys) override;

  // ── ECDH ────────────────────────────────────────────────────
  std::string ecdh_encrypt(const private_key_type& from_key,
                           const public_key_type& to_key,
                           const std::string& content,
                           std::optional<uint64_t> nonce) override;
  std::string ecdh_decrypt(key_finder_type key_finder,
                           const public_key_type& from_key,
                           const public_key_type& to_key,
                           const std::string& encrypted_content) override;
};

} // namespace beekeeper_minimal
