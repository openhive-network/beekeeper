#include <beekeeper_wasm/beekeeper_wasm_api.hpp>

#include <emscripten/val.h>

#include <stdexcept>

namespace beekeeper_wasm {

// ── js_callback_storage ───────────────────────────────────

js_callback_storage::js_callback_storage(emscripten::val storage)
  : save_fn_(storage["save_fn"])
  , load_fn_(storage["load_fn"])
  , list_dir_fn_(storage["list_dir_fn"])
{
}

void js_callback_storage::save(const std::string& name, const std::vector<char>& buffer)
{
  auto js_uint8 = emscripten::val::global("Uint8Array").new_(buffer.size());
  auto mem_view = emscripten::val(emscripten::typed_memory_view(buffer.size(),
                    reinterpret_cast<const uint8_t*>(buffer.data())));
  js_uint8.call<void>("set", mem_view);

  save_fn_(std::string(name), js_uint8);
}

std::vector<char> js_callback_storage::load(const std::string& name)
{
  emscripten::val result = load_fn_(std::string(name));

  unsigned len = result["length"].as<unsigned>();
  std::vector<char> buf(len);
  emscripten::val(emscripten::typed_memory_view(len, reinterpret_cast<uint8_t*>(buf.data())))
    .call<void>("set", result);
  return buf;
}

bool js_callback_storage::scan_dir(const std::string& wallet_name)
{
  emscripten::val result = list_dir_fn_();

  unsigned len = result["length"].as<unsigned>();
  for (unsigned i = 0; i < len; ++i)
    if (result[i].as<std::string>() == wallet_name)
      return true;
  return false;
}

// ── beekeeper_api ──────────────────────────────────────────

beekeeper_api::beekeeper_api(emscripten::val storage, emscripten::val crypto, uint32_t unlock_timeout)
  : crypto_(std::move(crypto))
  , storage_(std::move(storage))
  , bk_(crypto_, storage_, unlock_timeout)
{
}

// ── session ────────────────────────────────────────────────

std::string beekeeper_api::create_session(const std::string& /*salt*/)
{
  // salt is accepted for API compatibility but not used — tokens are
  // generated from cryptographically secure random bytes.
  return bk_.create_session();
}

void beekeeper_api::close_session(const std::string& token)
{
  bk_.close_session(token);
}

// ── wallet lifecycle ───────────────────────────────────────

std::string beekeeper_api::create(const std::string& token, const std::string& wallet_name, const std::string& password, bool is_temporary)
{
  bk_.validate_token(token);
  return bk_.create_wallet(token, wallet_name, password, is_temporary);
}

void beekeeper_api::open(const std::string& token, const std::string& wallet_name)
{
  bk_.validate_token(token);
  bk_.open_wallet(token, wallet_name);
}

void beekeeper_api::close(const std::string& token, const std::string& wallet_name)
{
  bk_.validate_token(token);
  bk_.close_wallet(wallet_name);
}

void beekeeper_api::lock(const std::string& token, const std::string& wallet_name)
{
  bk_.validate_token(token);
  bk_.lock(wallet_name);
}

void beekeeper_api::lock_all(const std::string& token)
{
  bk_.validate_token(token);
  bk_.lock_all();
}

void beekeeper_api::unlock(const std::string& token, const std::string& wallet_name, const std::string& password)
{
  bk_.validate_token(token);
  bk_.unlock(wallet_name, password);
}

// ── key management ─────────────────────────────────────────

std::string beekeeper_api::import_key(const std::string& token, const std::string& wallet_name, const std::string& wif_key)
{
  bk_.validate_token(token);
  return bk_.import_key(wallet_name, wif_key, prefix_);
}

void beekeeper_api::remove_key(const std::string& token, const std::string& wallet_name, const std::string& public_key)
{
  bk_.validate_token(token);
  auto pk = crypto_.public_key_from_string(public_key, prefix_);
  bk_.remove_key(wallet_name, pk);
}

emscripten::val beekeeper_api::get_public_keys(const std::string& token, const std::string& wallet_name)
{
  bk_.validate_token(token);
  auto keys = bk_.get_public_keys(wallet_name);
  auto arr = emscripten::val::array();
  for (auto& kv : keys)
    arr.call<void>("push", crypto_.public_key_to_string(kv.first, kv.second.second));
  return arr;
}

// ── signing ────────────────────────────────────────────────

std::string beekeeper_api::sign_digest(const std::string& token, const std::string& sig_digest, const std::string& public_key, const std::string& wallet_name)
{
  bk_.validate_token(token);
  auto pk = crypto_.public_key_from_string(public_key, prefix_);
  auto digest = crypto_.digest_from_hex(sig_digest);
  auto sig = bk_.sign_digest(wallet_name, digest, pk, prefix_);
  return crypto_.signature_to_hex(sig);
}

// ── encrypt / decrypt ───────────────────────────────────────

std::string beekeeper_api::encrypt_data(const std::string& token, const std::string& wallet_name,
                                        const std::string& from_key, const std::string& to_key,
                                        const std::string& content, double nonce)
{
  bk_.validate_token(token);
  auto from_pk = crypto_.public_key_from_string(from_key, prefix_);
  auto to_pk = crypto_.public_key_from_string(to_key, prefix_);
  // JS number (double) preserves integers exactly up to 2^53, sufficient for
  // microsecond timestamps.  Previous uint32_t silently truncated 64-bit nonces.
  return bk_.encrypt_data(wallet_name, from_pk, to_pk, content, prefix_, static_cast<uint64_t>(nonce));
}

std::string beekeeper_api::decrypt_data(const std::string& token, const std::string& wallet_name,
                                        const std::string& from_key, const std::string& to_key,
                                        const std::string& encrypted_content)
{
  bk_.validate_token(token);
  auto from_pk = crypto_.public_key_from_string(from_key, prefix_);
  auto to_pk = crypto_.public_key_from_string(to_key, prefix_);
  return bk_.decrypt_data(wallet_name, from_pk, to_pk, encrypted_content, prefix_);
}

// ── query ──────────────────────────────────────────────────

bool beekeeper_api::has_matching_private_key(const std::string& token, const std::string& wallet_name, const std::string& public_key)
{
  bk_.validate_token(token);
  auto pk = crypto_.public_key_from_string(public_key, prefix_);
  return bk_.has_private_key(wallet_name, pk);
}

bool beekeeper_api::has_wallet(const std::string& token, const std::string& wallet_name)
{
  bk_.validate_token(token);
  return bk_.has_wallet(wallet_name);
}

beekeeper_minimal::session_info beekeeper_api::get_info(const std::string& token)
{
  bk_.validate_token(token);
  // Timeout is enforced by the TS layer (BeekeeperLockedWallet.unlocked getter).
  // No C++ check_timeout() here — WASM does not use C++ timers.
  return bk_.get_info();
}

} // namespace beekeeper_wasm
