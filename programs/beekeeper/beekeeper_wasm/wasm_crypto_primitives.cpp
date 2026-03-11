#include <beekeeper_wasm/wasm_crypto_primitives.hpp>

#include "standalone_ripemd160.hpp"
#include "standalone_base58.h"

#include <secp256k1.h>
#include <secp256k1_recovery.h>

#include <cstring>
#include <stdexcept>

using emscripten::val;

namespace beekeeper_wasm {

// ── wasm_crypto_provider (wires primitives → impl) ────────────

wasm_crypto_provider::wasm_crypto_provider(val crypto_obj)
  : wasm_crypto_provider_prims_holder(std::move(crypto_obj))
  , crypto_provider_impl(wasm_crypto_provider_prims_holder::prims_)
{
}

// ── wasm_crypto_primitives ────────────────────────────────────

wasm_crypto_primitives::wasm_crypto_primitives(val crypto_obj)
  : crypto_(std::move(crypto_obj))
  , secp_ctx_(secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY))
{
}

wasm_crypto_primitives::~wasm_crypto_primitives()
{
  secp256k1_context_destroy(secp_ctx_);
}

// ── Helpers ────────────────────────────────────────────────────

val wasm_crypto_primitives::to_js_array(const uint8_t* data, size_t len) const
{
  auto js_arr = val::global("Uint8Array").new_(static_cast<unsigned>(len));
  auto view = val(emscripten::typed_memory_view(len, data));
  js_arr.call<void>("set", view);
  return js_arr;
}

template<size_t N>
beekeeper_minimal::byte_array<N> wasm_crypto_primitives::from_js_fixed(val js_arr) const
{
  beekeeper_minimal::byte_array<N> result;
  emscripten::val(emscripten::typed_memory_view(N, result.data.data()))
    .call<void>("set", js_arr);
  return result;
}

std::vector<uint8_t> wasm_crypto_primitives::from_js_vector(val js_arr) const
{
  unsigned len = js_arr["length"].as<unsigned>();
  std::vector<uint8_t> result(len);
  emscripten::val(emscripten::typed_memory_view(len, result.data()))
    .call<void>("set", js_arr);
  return result;
}

// ── JS-delegated (async via Asyncify) ─────────────────────────

beekeeper_minimal::digest_type wasm_crypto_primitives::sha256(const uint8_t* data, size_t len)
{
  auto promise = crypto_.call<val>("sha256", to_js_array(data, len));
  auto result = promise.await();
  return from_js_fixed<32>(result);
}

beekeeper_minimal::sha512_hash wasm_crypto_primitives::sha512(const uint8_t* data, size_t len)
{
  auto promise = crypto_.call<val>("sha512", to_js_array(data, len));
  auto result = promise.await();
  return from_js_fixed<64>(result);
}

std::vector<uint8_t> wasm_crypto_primitives::aes256_cbc_encrypt(
    const uint8_t* key, const uint8_t* iv,
    const uint8_t* data, size_t len)
{
  auto promise = crypto_.call<val>("aes256CbcEncrypt",
    to_js_array(key, 32),
    to_js_array(iv, 16),
    to_js_array(data, len));
  auto result = promise.await();
  return from_js_vector(result);
}

std::vector<uint8_t> wasm_crypto_primitives::aes256_cbc_decrypt(
    const uint8_t* key, const uint8_t* iv,
    const uint8_t* data, size_t len)
{
  auto promise = crypto_.call<val>("aes256CbcDecrypt",
    to_js_array(key, 32),
    to_js_array(iv, 16),
    to_js_array(data, len));
  auto result = promise.await();

  // JS returns null when SubtleCrypto.decrypt() fails (e.g. wrong password / bad padding).
  // We must check here because Asyncify's val::await() aborts on rejected Promises —
  // _emval_await (libemval.js) uses bare `await` with no rejection handler.
  // Note: exceptions thrown after val::await() in an Asyncify context may bypass intermediate
  // C++ catch(...) blocks during stack unwinding, so we use the final user-facing message here.
  if (result.isNull())
    throw std::runtime_error("Invalid password");

  return from_js_vector(result);
}

// ── Native secp256k1 ──────────────────────────────────────────

void wasm_crypto_primitives::get_random_bytes(uint8_t* buf, size_t len)
{
  // JS callback fills the WASM heap view in place — zero-copy
  crypto_.call<void>("getRandomBytes",
      emscripten::val(emscripten::typed_memory_view(len, buf)));
}

beekeeper_minimal::private_key_type wasm_crypto_primitives::generate_private_key()
{
  beekeeper_minimal::private_key_type key;

  get_random_bytes(key.data.data(), 32);

  // Verify it's a valid secp256k1 scalar (retry if not — astronomically unlikely)
  if (!secp256k1_ec_seckey_verify(secp_ctx_, key.data.data()))
    return generate_private_key();

  return key;
}

beekeeper_minimal::public_key_type wasm_crypto_primitives::get_public_key(
    const beekeeper_minimal::private_key_type& privkey)
{
  secp256k1_pubkey pubkey;
  if (!secp256k1_ec_pubkey_create(secp_ctx_, &pubkey, privkey.data.data()))
    throw std::runtime_error("Invalid private key");

  beekeeper_minimal::public_key_type result;
  size_t pk_len = 33;
  secp256k1_ec_pubkey_serialize(secp_ctx_, result.data.data(), &pk_len,
                                 &pubkey, SECP256K1_EC_COMPRESSED);
  return result;
}

bool wasm_crypto_primitives::is_canonical_signature(
    const beekeeper_minimal::signature_type& sig)
{
  // BIP-0062: S must be <= n/2 (low-S normalization).
  // n/2 for secp256k1 in big-endian bytes:
  static const uint8_t half_order[32] = {
    0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x5D, 0x57, 0x6E, 0x73, 0x57, 0xA4, 0x50, 0x1D,
    0xDF, 0xE9, 0x2F, 0x46, 0x68, 0x1B, 0x20, 0xA0
  };
  // S value is at bytes [33..64] of the 65-byte signature
  return std::memcmp(sig.data.data() + 33, half_order, 32) <= 0;
}

namespace {

// Nonce function that increments a counter on each retry, matching FC's behavior.
int extended_nonce_function(unsigned char* nonce32,
                            const unsigned char* msg32,
                            const unsigned char* key32,
                            const unsigned char* algo16,
                            void* data, unsigned int /*attempt*/)
{
  unsigned int* extra = static_cast<unsigned int*>(data);
  (*extra)++;
  return secp256k1_nonce_function_default(nonce32, msg32, key32, algo16, nullptr, *extra);
}

} // anonymous namespace

beekeeper_minimal::signature_type wasm_crypto_primitives::sign_compact(
    const beekeeper_minimal::private_key_type& privkey,
    const beekeeper_minimal::digest_type& digest)
{
  beekeeper_minimal::signature_type result;
  int recid;
  unsigned int counter = 0;

  do
  {
    secp256k1_ecdsa_recoverable_signature sig;
    if (!secp256k1_ecdsa_sign_recoverable(secp_ctx_, &sig,
          digest.data.data(), privkey.data.data(),
          extended_nonce_function, &counter))
      throw std::runtime_error("secp256k1_ecdsa_sign_recoverable failed");

    secp256k1_ecdsa_recoverable_signature_serialize_compact(
        secp_ctx_, result.data.data() + 1, &recid, &sig);
  }
  while (!is_canonical_signature(result));

  result.data[0] = static_cast<uint8_t>(27 + 4 + recid);
  return result;
}

beekeeper_minimal::sha512_hash wasm_crypto_primitives::ecdh_shared_secret(
    const beekeeper_minimal::private_key_type& privkey,
    const beekeeper_minimal::public_key_type& pubkey)
{
  // Matches FC's get_shared_secret:
  // 1. Parse the compressed public key
  secp256k1_pubkey pk;
  if (!secp256k1_ec_pubkey_parse(secp_ctx_, &pk, pubkey.data.data(), 33))
    throw std::runtime_error("Invalid public key for ECDH");

  // 2. Scalar multiply: pk = privkey * pk (ECDH)
  if (!secp256k1_ec_pubkey_tweak_mul(secp_ctx_, &pk, privkey.data.data()))
    throw std::runtime_error("ECDH scalar multiply failed");

  // 3. Serialize compressed (33 bytes)
  uint8_t compressed[33];
  size_t pk_len = 33;
  secp256k1_ec_pubkey_serialize(secp_ctx_, compressed, &pk_len,
                                 &pk, SECP256K1_EC_COMPRESSED);

  // 4. SHA-512 of x-coordinate only (bytes 1..32) — async via JS SubtleCrypto
  return sha512(compressed + 1, 32);
}

// ── Native standalone ─────────────────────────────────────────

std::array<uint8_t, 20> wasm_crypto_primitives::ripemd160(const uint8_t* data, size_t len)
{
  return standalone_ripemd160(data, len);
}

size_t wasm_crypto_primitives::base58_encode(const uint8_t* data, size_t data_len,
                                              char* out, size_t out_size)
{
  return standalone_base58_encode_c(data, data_len, out, out_size);
}

size_t wasm_crypto_primitives::base58_decode(const char* str, size_t str_len,
                                              uint8_t* out, size_t out_size)
{
  return standalone_base58_decode_c(str, str_len, out, out_size, [](const char* error) {
    throw std::runtime_error(error);
  });
}

} // namespace beekeeper_wasm
