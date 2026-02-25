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

  // ── AES-256-CBC (no padding) ─────────────────────────────────

  /// Encrypt data with AES-256-CBC, no PKCS7 padding.
  /// @param key  32-byte encryption key
  /// @param iv   16-byte initialization vector
  /// @param data plaintext (must be multiple of 16 bytes)
  virtual std::vector<uint8_t> aes256_cbc_encrypt(
      const uint8_t* key, const uint8_t* iv,
      const uint8_t* data, size_t len) = 0;

  /// Decrypt data with AES-256-CBC, no PKCS7 padding.
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
  virtual std::string base58_encode(const uint8_t* data, size_t len) = 0;

  /// Decode a Base58 string back to raw bytes.
  virtual std::vector<uint8_t> base58_decode(const std::string& str) = 0;
};

} // namespace beekeeper_minimal
