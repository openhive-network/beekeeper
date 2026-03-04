#pragma once

#include <core_minimal/beekeeper.hpp>
#include <core_minimal/wallet_storage.hpp>
#include <beekeeper_wasm/wasm_crypto_primitives.hpp>

#include <emscripten/val.h>

#include <string>
#include <vector>
#include <stdexcept>

namespace beekeeper_wasm {

/// wallet_storage implementation that delegates to a JS storage object.
/// The JS object must provide: save_fn, load_fn, list_dir_fn callbacks.
class js_callback_storage final : public beekeeper_minimal::wallet_storage
{
public:
  /// @param storage  JS object: { save_fn, load_fn, list_dir_fn }
  explicit js_callback_storage(emscripten::val storage);

  void save(const std::string& name, const std::vector<char>& buffer) override;
  std::vector<char> load(const std::string& name) override;
  std::vector<std::string> list_dir() override;

private:
  emscripten::val save_fn_;
  emscripten::val load_fn_;
  emscripten::val list_dir_fn_;
};

/// Thin embind-friendly API that wraps core_minimal::beekeeper.
/// Methods return native types (string, bool, structs) or void.
/// Errors propagate as C++ exceptions (embind converts to JS exceptions).
class beekeeper_api final
{
public:
  /// @param storage         JS object: { save_fn, load_fn, list_dir_fn }
  /// @param crypto          JS object with async hash/AES methods
  /// @param unlock_timeout  Inactivity timeout in seconds (default 900)
  beekeeper_api(emscripten::val storage, emscripten::val crypto, uint32_t unlock_timeout);

  // ── session ──────────────────────────────────────────────

  std::string create_session(const std::string& salt);
  void close_session(const std::string& token);

  // ── wallet lifecycle ─────────────────────────────────────

  std::string create(const std::string& token, const std::string& wallet_name, const std::string& password, bool is_temporary);

  void open(const std::string& token, const std::string& wallet_name);
  void close(const std::string& token, const std::string& wallet_name);

  void lock(const std::string& token, const std::string& wallet_name);
  void lock_all(const std::string& token);
  void unlock(const std::string& token, const std::string& wallet_name, const std::string& password);

  // ── key management ───────────────────────────────────────

  std::string import_key(const std::string& token, const std::string& wallet_name, const std::string& wif_key);
  void remove_key(const std::string& token, const std::string& wallet_name, const std::string& public_key);

  std::vector<std::string> get_public_keys(const std::string& token, const std::string& wallet_name);

  // ── signing ──────────────────────────────────────────────

  std::string sign_digest(const std::string& token, const std::string& sig_digest, const std::string& public_key, const std::string& wallet_name);

  // ── encrypt / decrypt ────────────────────────────────────

  std::string encrypt_data(const std::string& token, const std::string& wallet_name,
                           const std::string& from_key, const std::string& to_key,
                           const std::string& content, uint32_t nonce);
  std::string decrypt_data(const std::string& token, const std::string& wallet_name,
                           const std::string& from_key, const std::string& to_key,
                           const std::string& encrypted_content);

  // ── query ────────────────────────────────────────────────

  bool has_matching_private_key(const std::string& token, const std::string& wallet_name, const std::string& public_key);
  bool has_wallet(const std::string& token, const std::string& wallet_name);
  std::vector<beekeeper_minimal::wallet_details> list_wallets(const std::string& token);
  beekeeper_minimal::session_info get_info(const std::string& token);

private:
  static constexpr const char* prefix_ = "STM";

  wasm_crypto_provider                   crypto_;
  js_callback_storage                    storage_;
  beekeeper_minimal::beekeeper           bk_;
};

} // namespace beekeeper_wasm
