#pragma once

/// Composite crypto_primitives for WASM builds:
///   - sha256, sha512, aes256_cbc_encrypt/decrypt: async JS via SubtleCrypto (val::await)
///   - secp256k1 ops: native C compiled to WASM (secp256k1-zkp)
///   - ripemd160, base58: standalone C++ compiled to WASM
///
/// JS callbacks return Promises. C++ unwraps them with emscripten::val::await()
/// which requires the WASM module to be built with -sASYNCIFY.
///
/// JS object must provide:
///   sha256(data: Uint8Array): Promise<Uint8Array>         // → 32 bytes
///   sha512(data: Uint8Array): Promise<Uint8Array>         // → 64 bytes
///   aes256CbcEncrypt(key32, iv16, data): Promise<Uint8Array>
///   aes256CbcDecrypt(key32, iv16, data): Promise<Uint8Array>

#include <core_minimal/crypto_provider_impl.hpp>
#include <core_minimal/crypto_primitives.hpp>

#include <emscripten/val.h>

struct secp256k1_context_struct;

namespace beekeeper_wasm {

class wasm_crypto_primitives final : public beekeeper_minimal::crypto_primitives
{
public:
  explicit wasm_crypto_primitives(emscripten::val crypto_obj);
  ~wasm_crypto_primitives();

  // ── JS-delegated (async, uses val::await) ────────────────────

  beekeeper_minimal::digest_type sha256(const uint8_t* data, size_t len) override;
  beekeeper_minimal::sha512_hash sha512(const uint8_t* data, size_t len) override;

  std::vector<uint8_t> aes256_cbc_encrypt(
      const uint8_t* key, const uint8_t* iv,
      const uint8_t* data, size_t len) override;
  std::vector<uint8_t> aes256_cbc_decrypt(
      const uint8_t* key, const uint8_t* iv,
      const uint8_t* data, size_t len) override;

  // ── Native secp256k1-zkp (compiled to WASM) ─────────────────

  beekeeper_minimal::private_key_type generate_private_key() override;
  beekeeper_minimal::public_key_type get_public_key(
      const beekeeper_minimal::private_key_type& privkey) override;
  beekeeper_minimal::signature_type sign_compact(
      const beekeeper_minimal::private_key_type& privkey,
      const beekeeper_minimal::digest_type& digest) override;
  beekeeper_minimal::sha512_hash ecdh_shared_secret(
      const beekeeper_minimal::private_key_type& privkey,
      const beekeeper_minimal::public_key_type& pubkey) override;

  // ── Native standalone (compiled to WASM) ─────────────────────

  std::array<uint8_t, 20> ripemd160(const uint8_t* data, size_t len) override;
  std::string base58_encode(const uint8_t* data, size_t len) override;
  std::vector<uint8_t> base58_decode(const std::string& str) override;

  // ── Random (synchronous JS crypto.getRandomValues) ─────────

  void get_random_bytes(uint8_t* buf, size_t len) override;

private:
  emscripten::val to_js_array(const uint8_t* data, size_t len) const;

  template<size_t N>
  beekeeper_minimal::byte_array<N> from_js_fixed(emscripten::val js_arr) const;

  std::vector<uint8_t> from_js_vector(emscripten::val js_arr) const;

  static bool is_canonical_signature(const beekeeper_minimal::signature_type& sig);

  emscripten::val crypto_;
  secp256k1_context_struct* secp_ctx_;
};

/// Private base that ensures prims_ is fully constructed before
/// crypto_provider_impl binds a reference to it (base-from-member idiom).
struct wasm_crypto_provider_prims_holder
{
  explicit wasm_crypto_provider_prims_holder(emscripten::val crypto_obj)
    : prims_(std::move(crypto_obj)) {}
protected:
  wasm_crypto_primitives prims_;
};

/// Full crypto_provider for WASM: primitives → crypto_provider_impl.
class wasm_crypto_provider final
  : private wasm_crypto_provider_prims_holder
  , public  beekeeper_minimal::crypto_provider_impl
{
public:
  explicit wasm_crypto_provider(emscripten::val crypto_obj);
};

} // namespace beekeeper_wasm
