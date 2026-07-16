#pragma once

#include <core_minimal/crypto_types.hpp>

#include <optional>
#include <string>
#include <vector>

namespace beekeeper_minimal {

/// Abstract interface for all cryptographic operations.
/// The consumer provides the implementation (e.g. FC-backed, WebCrypto-backed).
/// Modeled after wallet_storage: a pure virtual interface.
struct crypto_provider
{
  virtual ~crypto_provider() = default;

  // ── Key operations ──────────────────────────────────────────

  /// Generate a new random private key.
  virtual private_key_type generate_private_key() = 0;

  /// Decode a WIF-encoded private key string.
  /// Returns nullopt if the WIF is invalid.
  virtual std::optional<private_key_type> wif_to_key(const std::string& wif) = 0;

  /// Encode a private key as WIF string.
  virtual std::string key_to_wif(const private_key_type& key) = 0;

  /// Derive the (compressed) public key from a private key.
  virtual public_key_type get_public_key(const private_key_type& key) = 0;

  /// Encode a public key as a prefixed Base58 string (e.g., "STM...").
  virtual std::string public_key_to_string(const public_key_type& key,
                                           const std::string& prefix) = 0;

  /// Parse a prefixed Base58 public key string.
  virtual public_key_type public_key_from_string(const std::string& str,
                                                 const std::string& prefix) = 0;

  // ── Signing ─────────────────────────────────────────────────

  /// Sign a digest with a private key. Returns a compact (65-byte) recoverable signature.
  virtual signature_type sign_compact(const private_key_type& key,
                                      const digest_type& digest) = 0;

  // ── Digest / signature hex ──────────────────────────────────

  /// Parse a hex-encoded digest string (64 hex chars) into a digest_type.
  virtual digest_type digest_from_hex(const std::string& hex) = 0;

  /// Convert a signature to its hex string representation.
  virtual std::string signature_to_hex(const signature_type& sig) = 0;

  // ── Wallet encryption (high-level) ─────────────────────────

  /// Encrypt wallet keys: password + key pairs -> raw AES ciphertext.
  virtual std::vector<char> encrypt_wallet_keys(
      const std::string& password,
      const keys_map& keys) = 0;

  /// Encrypt wallet data: password + key pairs -> wallet file content (JSON bytes).
  /// Convenience wrapper: calls encrypt_wallet_keys() then wraps as JSON hex string.
  virtual std::vector<char> encrypt_wallet_data(
      const std::string& password,
      const keys_map& keys) = 0;

  /// Decrypt wallet data: password + cipher_keys blob -> key pairs.
  /// Throws on invalid password.
  virtual keys_map decrypt_wallet_data(
      const std::string& password,
      const std::vector<char>& cipher_keys) = 0;

  /// Extract the cipher_keys blob from a wallet file (JSON parsing only, no decryption).
  virtual std::vector<char> parse_wallet_file(
      const std::vector<char>& wallet_file_content) = 0;

  /// Validate a password against an encrypted wallet blob.
  /// Throws on invalid password.
  virtual void validate_password(
      const std::string& password,
      const std::vector<char>& cipher_keys) = 0;

  /// True if the blob uses the legacy (unsalted SHA-512) format and should be
  /// re-encrypted with the current scheme on the next successful unlock.
  virtual bool is_legacy_wallet(const std::vector<char>& cipher_keys) const = 0;

  // ── ECDH encrypt / decrypt ─────────────────────────────────

  /// Encrypt a message using ECDH shared secret.
  virtual std::string ecdh_encrypt(
      const private_key_type& from_key,
      const public_key_type& to_key,
      const std::string& content,
      std::optional<uint64_t> nonce = {}) = 0;

  // ── Random ────────────────────────────────────────────────

  /// Fill buffer with cryptographically secure random bytes.
  virtual void get_random_bytes(uint8_t* buf, size_t len) = 0;

  /// Decrypt a message using ECDH shared secret.
  /// The caller resolves the private key beforehand (no callback needed).
  virtual std::string ecdh_decrypt(
      const private_key_type& priv_key,
      const public_key_type& other_pub,
      const std::string& encrypted_content) = 0;
};

} // namespace beekeeper_minimal
