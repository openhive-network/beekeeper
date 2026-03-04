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
  // Create a JS Uint8Array copy of the C++ buffer
  auto js_uint8 = emscripten::val::global("Uint8Array").new_(buffer.size());
  // Copy data into the JS array
  auto mem_view = emscripten::val(emscripten::typed_memory_view(buffer.size(),
                    reinterpret_cast<const uint8_t*>(buffer.data())));
  js_uint8.call<void>("set", mem_view);

  // .await() handles both sync and async (Promise) callbacks via Asyncify.
  // Sync callbacks return a non-thenable value — await() returns it immediately.
  save_fn_(std::string(name), js_uint8).await();
}

std::vector<char> js_callback_storage::load(const std::string& name)
{
  // .await() handles both sync and async (Promise) callbacks via Asyncify.
  emscripten::val result = load_fn_(std::string(name)).await();

  // Convert JS Uint8Array back to vector<char>
  unsigned len = result["length"].as<unsigned>();
  std::vector<char> buf(len);
  emscripten::val(emscripten::typed_memory_view(len, reinterpret_cast<uint8_t*>(buf.data())))
    .call<void>("set", result);
  return buf;
}

std::vector<std::string> js_callback_storage::list_dir()
{
  // .await() handles both sync and async (Promise) callbacks via Asyncify.
  emscripten::val result = list_dir_fn_().await();

  unsigned len = result["length"].as<unsigned>();
  std::vector<std::string> names;
  names.reserve(len);
  for (unsigned i = 0; i < len; ++i)
    names.push_back(result[i].as<std::string>());
  return names;
}

// ── beekeeper_api ──────────────────────────────────────────

beekeeper_api::beekeeper_api(emscripten::val storage, emscripten::val crypto, uint32_t unlock_timeout)
  : crypto_(std::move(crypto))
  , storage_(std::move(storage))
  , bk_(crypto_, storage_, unlock_timeout)
{
}

// ── session ────────────────────────────────────────────────

std::string beekeeper_api::create_session()
{
  return bk_.create_session();
}

std::string beekeeper_api::create_session(const std::string& salt)
{
  return bk_.create_session(salt);
}

void beekeeper_api::close_session(const std::string& token)
{
  bk_.close_session(token);
}

// ── wallet lifecycle ───────────────────────────────────────

std::string beekeeper_api::create(const std::string& token, const std::string& wallet_name)
{
  auto& ses = bk_.get_session(token);
  return ses.create_wallet(wallet_name, "");
}

std::string beekeeper_api::create(const std::string& token, const std::string& wallet_name, const std::string& password)
{
  auto& ses = bk_.get_session(token);
  return ses.create_wallet(wallet_name, password);
}

std::string beekeeper_api::create(const std::string& token, const std::string& wallet_name, const std::string& password, bool is_temporary)
{
  auto& ses = bk_.get_session(token);
  return ses.create_wallet(wallet_name, password, is_temporary);
}

void beekeeper_api::open(const std::string& token, const std::string& wallet_name)
{
  auto& ses = bk_.get_session(token);
  ses.open_wallet(wallet_name);
}

void beekeeper_api::close(const std::string& token, const std::string& wallet_name)
{
  auto& ses = bk_.get_session(token);
  ses.close_wallet(wallet_name);
}

void beekeeper_api::lock(const std::string& token, const std::string& wallet_name)
{
  auto& ses = bk_.get_session(token);
  ses.lock(wallet_name);
}

void beekeeper_api::lock_all(const std::string& token)
{
  auto& ses = bk_.get_session(token);
  ses.lock_all();
}

void beekeeper_api::unlock(const std::string& token, const std::string& wallet_name, const std::string& password)
{
  auto& ses = bk_.get_session(token);
  ses.unlock(wallet_name, password);
}

// ── key management ─────────────────────────────────────────

std::string beekeeper_api::import_key(const std::string& token, const std::string& wallet_name, const std::string& wif_key)
{
  auto& ses = bk_.get_session(token);
  return ses.import_key(wallet_name, wif_key, prefix_);
}

void beekeeper_api::remove_key(const std::string& token, const std::string& wallet_name, const std::string& public_key)
{
  auto& ses = bk_.get_session(token);
  auto pk = crypto_.public_key_from_string(public_key, prefix_);
  ses.remove_key(wallet_name, pk);
}

std::vector<std::string> beekeeper_api::get_public_keys(const std::string& token)
{
  auto& ses = bk_.get_session(token);
  auto keys = ses.get_public_keys("");  // empty name → merge all unlocked
  std::vector<std::string> result;
  result.reserve(keys.size());
  for (auto& kv : keys)
    result.push_back(crypto_.public_key_to_string(kv.first, kv.second.second));
  return result;
}

std::vector<std::string> beekeeper_api::get_public_keys(const std::string& token, const std::string& wallet_name)
{
  auto& ses = bk_.get_session(token);
  auto keys = ses.get_public_keys(wallet_name);
  std::vector<std::string> result;
  result.reserve(keys.size());
  for (auto& kv : keys)
    result.push_back(crypto_.public_key_to_string(kv.first, kv.second.second));
  return result;
}

// ── signing ────────────────────────────────────────────────

std::string beekeeper_api::sign_digest(const std::string& token, const std::string& sig_digest, const std::string& public_key)
{
  auto& ses = bk_.get_session(token);
  auto pk = crypto_.public_key_from_string(public_key, prefix_);
  auto digest = crypto_.digest_from_hex(sig_digest);
  auto sig = ses.sign_digest("", digest, pk, prefix_);  // empty wallet name → search all
  return crypto_.signature_to_hex(sig);
}

std::string beekeeper_api::sign_digest(const std::string& token, const std::string& sig_digest, const std::string& public_key, const std::string& wallet_name)
{
  auto& ses = bk_.get_session(token);
  auto pk = crypto_.public_key_from_string(public_key, prefix_);
  auto digest = crypto_.digest_from_hex(sig_digest);
  auto sig = ses.sign_digest(wallet_name, digest, pk, prefix_);
  return crypto_.signature_to_hex(sig);
}

// ── encrypt / decrypt ───────────────────────────────────────

std::string beekeeper_api::encrypt_data(const std::string& token, const std::string& wallet_name,
                                        const std::string& from_key, const std::string& to_key,
                                        const std::string& content)
{
  auto& ses = bk_.get_session(token);
  auto from_pk = crypto_.public_key_from_string(from_key, prefix_);
  auto to_pk = crypto_.public_key_from_string(to_key, prefix_);
  return ses.encrypt_data(wallet_name, from_pk, to_pk, content, prefix_);
}

std::string beekeeper_api::encrypt_data(const std::string& token, const std::string& wallet_name,
                                        const std::string& from_key, const std::string& to_key,
                                        const std::string& content, uint32_t nonce)
{
  auto& ses = bk_.get_session(token);
  auto from_pk = crypto_.public_key_from_string(from_key, prefix_);
  auto to_pk = crypto_.public_key_from_string(to_key, prefix_);
  return ses.encrypt_data(wallet_name, from_pk, to_pk, content, prefix_, static_cast<uint64_t>(nonce));
}

std::string beekeeper_api::decrypt_data(const std::string& token, const std::string& wallet_name,
                                        const std::string& from_key, const std::string& to_key,
                                        const std::string& encrypted_content)
{
  auto& ses = bk_.get_session(token);
  auto from_pk = crypto_.public_key_from_string(from_key, prefix_);
  auto to_pk = crypto_.public_key_from_string(to_key, prefix_);
  return ses.decrypt_data(wallet_name, from_pk, to_pk, encrypted_content, prefix_);
}

// ── query ──────────────────────────────────────────────────

bool beekeeper_api::has_matching_private_key(const std::string& token, const std::string& wallet_name, const std::string& public_key)
{
  auto& ses = bk_.get_session(token);
  auto keys = ses.get_public_keys(wallet_name);
  auto pk = crypto_.public_key_from_string(public_key, prefix_);
  return keys.find(pk) != keys.end();
}

bool beekeeper_api::has_wallet(const std::string& token, const std::string& wallet_name)
{
  auto& ses = bk_.get_session(token);
  return ses.has_wallet(wallet_name);
}

std::vector<beekeeper_minimal::wallet_details> beekeeper_api::list_wallets(const std::string& token)
{
  auto& ses = bk_.get_session(token);
  return ses.list_wallets();
}

beekeeper_minimal::session_info beekeeper_api::get_info(const std::string& token)
{
  bk_.check_timeouts();
  auto& ses = bk_.get_session(token);
  return ses.get_info();
}

} // namespace beekeeper_wasm
