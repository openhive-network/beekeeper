#pragma once

/// Low-level cryptographic primitives interface.
/// Implementations provide the actual crypto (FC-backed, JS-backed, etc.).
/// The higher-level crypto_provider_impl class uses these to build
/// wallet encryption, ECDH messaging, WIF encoding, etc.

#include <core_minimal/crypto_types.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace beekeeper_minimal {

struct crypto_primitives
{
  virtual ~crypto_primitives() = default;

  // ── Hashing ──────────────────────────────────────────────────

  virtual digest_type sha256(const uint8_t* data, size_t len) = 0;
  virtual sha512_hash sha512(const uint8_t* data, size_t len) = 0;

  /// RIPEMD-160 hash (20 bytes). Used for public key checksums.
  virtual std::array<uint8_t, 20> ripemd160(const uint8_t* data, size_t len) = 0;

  /// HMAC-SHA256 (RFC 2104). Returns the 32-byte authentication tag.
  /// Used for wallet-blob integrity / password verification (encrypt-then-MAC).
  virtual digest_type hmac_sha256(const uint8_t* key, size_t key_len,
                                  const uint8_t* data, size_t data_len) = 0;

  // ── Key derivation ───────────────────────────────────────────

  /// PBKDF2-HMAC-SHA512 (RFC 8018). Derives dk_len bytes from a password + salt.
  /// Used to derive the wallet cipher/MAC keys from the wallet password.
  virtual std::vector<uint8_t> pbkdf2_hmac_sha512(
      const uint8_t* password, size_t password_len,
      const uint8_t* salt, size_t salt_len,
      uint32_t iterations, size_t dk_len) = 0;

  // ── AES-256-CBC (PKCS#7 padding) ────────────────────────────

  /// Encrypt data with AES-256-CBC + PKCS#7 padding.
  /// Matches FC's standalone aes_encrypt() which uses OpenSSL defaults.
  /// @param key  32-byte encryption key
  /// @param iv   16-byte initialization vector
  /// @param data plaintext (any length; PKCS#7 padding is applied)
  virtual std::vector<uint8_t> aes256_cbc_encrypt(
      const uint8_t* key, const uint8_t* iv,
      const uint8_t* data, size_t len) = 0;

  /// Decrypt data with AES-256-CBC, removing PKCS#7 padding.
  virtual std::vector<uint8_t> aes256_cbc_decrypt(
      const uint8_t* key, const uint8_t* iv,
      const uint8_t* data, size_t len) = 0;

  // ── secp256k1 ────────────────────────────────────────────────

  /// Generate a new random secp256k1 private key.
  virtual private_key_type generate_private_key() = 0;

  /// Derive compressed public key (33 bytes) from private key.
  virtual public_key_type get_public_key(const private_key_type& privkey) = 0;

  /// ECDSA sign with recovery. Returns 65 bytes: [recovery_id+27+4, R(32), S(32)].
  /// Must produce canonical (low-S) signatures.
  virtual signature_type sign_compact(const private_key_type& privkey,
                                       const digest_type& digest) = 0;

  /// ECDH shared secret: SHA512(raw_ECDH_point). Returns 64 bytes.
  virtual sha512_hash ecdh_shared_secret(const private_key_type& privkey,
                                          const public_key_type& pubkey) = 0;

  // ── Base58 ───────────────────────────────────────────────────

  /// Encode raw bytes as a Base58 string (Bitcoin alphabet, no checksum).
  /// Writes into out_buf (NUL-terminated). Returns chars written (excl. NUL), 0 on error.
  /// Safe upper bound for out_size: data_len * 138 / 100 + 2
  virtual size_t base58_encode(const uint8_t* data, size_t data_len,
                                char* out, size_t out_size) = 0;

  /// Decode a Base58 string back to raw bytes.
  /// Returns bytes written, 0 on error (invalid char or buffer too small).
  /// Safe upper bound for out_size: str_len * 733 / 1000 + 2
  virtual size_t base58_decode(const char* str, size_t str_len,
                                uint8_t* out, size_t out_size) = 0;

  // ── Random ─────────────────────────────────────────────────

  /// Fill buffer with cryptographically secure random bytes.
  virtual void get_random_bytes(uint8_t* buf, size_t len) = 0;
};

} // namespace beekeeper_minimal
