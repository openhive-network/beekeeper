#include <beekeeper_wasm/beekeeper_wasm_api.hpp>

#include <emscripten/bind.h>

using namespace beekeeper_wasm;
using namespace emscripten;

EMSCRIPTEN_BINDINGS(beekeeper_api_instance) {

  // ── value types ─────────────────────────────────────────
  // Embind auto-serializes these as plain JS objects (no wrapper class overhead).
  // register_vector types are JS-iterable via for...of / Array.from() / spread (Emscripten 5.0+).

  value_object<beekeeper_minimal::wallet_details>("WalletDetails")
    .field("name", &beekeeper_minimal::wallet_details::name)
    .field("unlocked", &beekeeper_minimal::wallet_details::unlocked);

  value_object<beekeeper_minimal::session_info>("SessionInfo")
    .field("now", &beekeeper_minimal::session_info::now)
    .field("timeout_time", &beekeeper_minimal::session_info::timeout_time);

  register_vector<std::string>("StringVector");
  register_vector<beekeeper_minimal::wallet_details>("WalletDetailsVector");

  // ── main API class ──────────────────────────────────────

  class_<beekeeper_api>("beekeeper_api")

    .constructor<val, val, uint32_t>()

    // ── session ─────────────────────────────────────────

    .function("create_session(salt)", &beekeeper_api::create_session)
    .function("close_session(token)", &beekeeper_api::close_session)

    // ── wallet lifecycle ────────────────────────────────

    .function("create(token, wallet_name, password, is_temporary)", &beekeeper_api::create)
    .function("unlock(token, wallet_name, password)", &beekeeper_api::unlock)
    .function("open(token, wallet_name)", &beekeeper_api::open)
    .function("close(token, wallet_name)", &beekeeper_api::close)
    .function("lock_all(token)", &beekeeper_api::lock_all)
    .function("lock(token, wallet_name)", &beekeeper_api::lock)

    // ── key management ──────────────────────────────────

    .function("import_key(token, wallet_name, wif_key)", &beekeeper_api::import_key)
    .function("remove_key(token, wallet_name, public_key)", &beekeeper_api::remove_key)
    .function("get_public_keys(token, wallet_name)", &beekeeper_api::get_public_keys)

    // ── signing ─────────────────────────────────────────

    .function("sign_digest(token, sig_digest, public_key, wallet_name)", &beekeeper_api::sign_digest)

    // ── encrypt / decrypt ───────────────────────────────

    .function("encrypt_data(token, wallet_name, from_key, to_key, content, nonce)", &beekeeper_api::encrypt_data)
    .function("decrypt_data(token, wallet_name, from_key, to_key, encrypted_content)", &beekeeper_api::decrypt_data)

    // ── query ───────────────────────────────────────────

    .function("get_info(token)", &beekeeper_api::get_info)
    .function("has_matching_private_key(token, wallet_name, public_key)", &beekeeper_api::has_matching_private_key)
    .function("has_wallet(token, wallet_name)", &beekeeper_api::has_wallet)
    .function("list_wallets(token)", &beekeeper_api::list_wallets)
    ;
}

int main() {
  return 0;
}
