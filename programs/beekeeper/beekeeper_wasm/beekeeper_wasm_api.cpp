#include <beekeeper_wasm/beekeeper_wasm_api.hpp>

#include <emscripten/val.h>

#include <stdexcept>

namespace beekeeper_wasm {

// ── helpers ────────────────────────────────────────────────

namespace {

/// Escape a string for safe embedding in JSON.
std::string json_escape(const std::string& s)
{
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s)
  {
    switch (c)
    {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:   out += c;
    }
  }
  return out;
}

std::string ok_json(const std::string& inner_json)
{
  return "{\"result\":\"" + json_escape(inner_json) + "\"}";
}

std::string ok_empty()
{
  return "{\"result\":\"{}\"}";
}

} // anon

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
  for (unsigned i = 0; i < len; ++i)
    buf[i] = static_cast<char>(result[i].as<uint8_t>());
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

std::string beekeeper_api::error_json(const std::string& msg)
{
  return "{\"error\":\"" + json_escape(msg) + "\"}";
}

// ── session ────────────────────────────────────────────────

std::string beekeeper_api::create_session()
{
  return wrap([&]() {
    auto token = bk_.create_session();
    return ok_json("{\"token\":\"" + token + "\"}");
  });
}

std::string beekeeper_api::create_session(const std::string& salt)
{
  return wrap([&]() {
    auto token = bk_.create_session(salt);
    return ok_json("{\"token\":\"" + token + "\"}");
  });
}

std::string beekeeper_api::close_session(const std::string& token)
{
  return wrap([&]() {
    bk_.close_session(token);
    return ok_empty();
  });
}

// ── wallet lifecycle ───────────────────────────────────────

std::string beekeeper_api::create(const std::string& token, const std::string& wallet_name)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    auto pw = ses.create_wallet(wallet_name, "");
    return ok_json("{\"password\":\"" + json_escape(pw) + "\"}");
  });
}

std::string beekeeper_api::create(const std::string& token, const std::string& wallet_name, const std::string& password)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    auto pw = ses.create_wallet(wallet_name, password);
    return ok_json("{\"password\":\"" + json_escape(pw) + "\"}");
  });
}

std::string beekeeper_api::create(const std::string& token, const std::string& wallet_name, const std::string& password, bool is_temporary)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    auto pw = ses.create_wallet(wallet_name, password, is_temporary);
    return ok_json("{\"password\":\"" + json_escape(pw) + "\"}");
  });
}

std::string beekeeper_api::open(const std::string& token, const std::string& wallet_name)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    ses.open_wallet(wallet_name);
    return ok_empty();
  });
}

std::string beekeeper_api::close(const std::string& token, const std::string& wallet_name)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    ses.close_wallet(wallet_name);
    return ok_empty();
  });
}

std::string beekeeper_api::lock(const std::string& token, const std::string& wallet_name)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    ses.lock(wallet_name);
    return ok_empty();
  });
}

std::string beekeeper_api::lock_all(const std::string& token)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    ses.lock_all();
    return ok_empty();
  });
}

std::string beekeeper_api::unlock(const std::string& token, const std::string& wallet_name, const std::string& password)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    ses.unlock(wallet_name, password);
    return ok_empty();
  });
}

// ── key management ─────────────────────────────────────────

std::string beekeeper_api::import_key(const std::string& token, const std::string& wallet_name, const std::string& wif_key)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    auto pub = ses.import_key(wallet_name, wif_key, prefix_);
    return ok_json("{\"public_key\":\"" + json_escape(pub) + "\"}");
  });
}

std::string beekeeper_api::remove_key(const std::string& token, const std::string& wallet_name, const std::string& public_key)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    auto pk = crypto_.public_key_from_string(public_key, prefix_);
    ses.remove_key(wallet_name, pk);
    return ok_empty();
  });
}

std::string beekeeper_api::get_public_keys(const std::string& token)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    auto keys = ses.get_public_keys("");  // empty name → merge all unlocked
    std::string arr = "[";
    bool first = true;
    for (auto& kv : keys)
    {
      if (!first) arr += ",";
      first = false;
      arr += "{\"public_key\":\"" + crypto_.public_key_to_string(kv.first, kv.second.second) + "\"}";
    }
    arr += "]";
    return ok_json("{\"keys\":" + arr + "}");
  });
}

std::string beekeeper_api::get_public_keys(const std::string& token, const std::string& wallet_name)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    auto keys = ses.get_public_keys(wallet_name);
    std::string arr = "[";
    bool first = true;
    for (auto& kv : keys)
    {
      if (!first) arr += ",";
      first = false;
      arr += "{\"public_key\":\"" + crypto_.public_key_to_string(kv.first, kv.second.second) + "\"}";
    }
    arr += "]";
    return ok_json("{\"keys\":" + arr + "}");
  });
}

// ── signing ────────────────────────────────────────────────

std::string beekeeper_api::sign_digest(const std::string& token, const std::string& sig_digest, const std::string& public_key)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    auto pk = crypto_.public_key_from_string(public_key, prefix_);
    auto digest = crypto_.digest_from_hex(sig_digest);
    auto sig = ses.sign_digest("", digest, pk, prefix_);  // empty wallet name → search all
    auto sig_str = crypto_.signature_to_hex(sig);
    return ok_json("{\"signature\":\"" + sig_str + "\"}");
  });
}

std::string beekeeper_api::sign_digest(const std::string& token, const std::string& sig_digest, const std::string& public_key, const std::string& wallet_name)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    auto pk = crypto_.public_key_from_string(public_key, prefix_);
    auto digest = crypto_.digest_from_hex(sig_digest);
    auto sig = ses.sign_digest(wallet_name, digest, pk, prefix_);
    auto sig_str = crypto_.signature_to_hex(sig);
    return ok_json("{\"signature\":\"" + sig_str + "\"}");
  });
}

// ── encrypt / decrypt ───────────────────────────────────────

std::string beekeeper_api::encrypt_data(const std::string& token, const std::string& wallet_name,
                                        const std::string& from_key, const std::string& to_key,
                                        const std::string& content)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    auto from_pk = crypto_.public_key_from_string(from_key, prefix_);
    auto to_pk = crypto_.public_key_from_string(to_key, prefix_);
    auto encrypted = ses.encrypt_data(wallet_name, from_pk, to_pk, content, prefix_);
    return ok_json("{\"encrypted_content\":\"" + json_escape(encrypted) + "\"}");
  });
}

std::string beekeeper_api::encrypt_data(const std::string& token, const std::string& wallet_name,
                                        const std::string& from_key, const std::string& to_key,
                                        const std::string& content, uint32_t nonce)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    auto from_pk = crypto_.public_key_from_string(from_key, prefix_);
    auto to_pk = crypto_.public_key_from_string(to_key, prefix_);
    auto encrypted = ses.encrypt_data(wallet_name, from_pk, to_pk, content, prefix_, static_cast<uint64_t>(nonce));
    return ok_json("{\"encrypted_content\":\"" + json_escape(encrypted) + "\"}");
  });
}

std::string beekeeper_api::decrypt_data(const std::string& token, const std::string& wallet_name,
                                        const std::string& from_key, const std::string& to_key,
                                        const std::string& encrypted_content)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    auto from_pk = crypto_.public_key_from_string(from_key, prefix_);
    auto to_pk = crypto_.public_key_from_string(to_key, prefix_);
    auto decrypted = ses.decrypt_data(wallet_name, from_pk, to_pk, encrypted_content, prefix_);
    return ok_json("{\"decrypted_content\":\"" + json_escape(decrypted) + "\"}");
  });
}

// ── query ──────────────────────────────────────────────────

std::string beekeeper_api::has_matching_private_key(const std::string& token, const std::string& wallet_name, const std::string& public_key)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    auto keys = ses.get_public_keys(wallet_name);
    auto pk = crypto_.public_key_from_string(public_key, prefix_);
    bool found = keys.find(pk) != keys.end();
    return ok_json(std::string("{\"exists\":") + (found ? "true" : "false") + "}");
  });
}

std::string beekeeper_api::has_wallet(const std::string& token, const std::string& wallet_name)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    bool found = ses.has_wallet(wallet_name);
    return ok_json(std::string("{\"exists\":") + (found ? "true" : "false") + "}");
  });
}

std::string beekeeper_api::list_wallets(const std::string& token)
{
  return wrap([&]() {
    auto& ses = bk_.get_session(token);
    auto wallets = ses.list_wallets();
    std::string arr = "[";
    bool first = true;
    for (auto& wd : wallets)
    {
      if (!first) arr += ",";
      first = false;
      arr += "{\"name\":\"" + json_escape(wd.name) + "\",\"unlocked\":" + (wd.unlocked ? "true" : "false") + "}";
    }
    arr += "]";
    return ok_json("{\"wallets\":" + arr + "}");
  });
}

std::string beekeeper_api::get_info(const std::string& token)
{
  return wrap([&]() {
    bk_.check_timeouts();
    // Validate session exists
    auto& ses = bk_.get_session(token);
    (void)ses;
    auto now = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&now_t));
    std::string now_str(buf);
    return ok_json("{\"now\":\"" + now_str + "\",\"timeout_time\":\"" + now_str + "\"}");
  });
}

} // namespace beekeeper_wasm
