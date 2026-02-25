#pragma once

#include <core_minimal/beekeeper.hpp>
#include <core_minimal/wallet_storage.hpp>

#include <emscripten/val.h>

#include <string>
#include <vector>
#include <functional>

namespace beekeeper_wasm {

/// wallet_storage implementation that delegates to JS-side save/load callbacks.
/// The JS callbacks receive the wallet name and a Uint8Array buffer.
class js_callback_storage final : public beekeeper_minimal::wallet_storage
{
public:
  js_callback_storage(emscripten::val save_fn, emscripten::val load_fn);

  void save(const std::string& name, const std::vector<char>& buffer) override;
  std::vector<char> load(const std::string& name) override;

private:
  emscripten::val save_fn_;
  emscripten::val load_fn_;
};

/// Thin embind-friendly API that wraps core_minimal::beekeeper.
/// Every method returns a JSON string:
///   success → {"result": "<json>"}
///   failure → {"error": "<message>"}
class beekeeper_api final
{
public:
  /// @param save_fn         JS callback: (name: string, data: Uint8Array) => void
  /// @param load_fn         JS callback: (name: string) => Uint8Array  (throws if not found)
  /// @param unlock_timeout  Inactivity timeout in seconds (default 900)
  beekeeper_api(emscripten::val save_fn, emscripten::val load_fn, uint32_t unlock_timeout);

  // ── session ──────────────────────────────────────────────

  std::string create_session();
  std::string create_session(const std::string& salt);
  std::string close_session(const std::string& token);

  // ── wallet lifecycle ─────────────────────────────────────

  std::string create(const std::string& token, const std::string& wallet_name);
  std::string create(const std::string& token, const std::string& wallet_name, const std::string& password);

  std::string open(const std::string& token, const std::string& wallet_name);
  std::string close(const std::string& token, const std::string& wallet_name);

  std::string lock(const std::string& token, const std::string& wallet_name);
  std::string lock_all(const std::string& token);
  std::string unlock(const std::string& token, const std::string& wallet_name, const std::string& password);

  // ── key management ───────────────────────────────────────

  std::string import_key(const std::string& token, const std::string& wallet_name, const std::string& wif_key);
  std::string remove_key(const std::string& token, const std::string& wallet_name, const std::string& public_key);

  std::string get_public_keys(const std::string& token);
  std::string get_public_keys(const std::string& token, const std::string& wallet_name);

  // ── signing ──────────────────────────────────────────────

  std::string sign_digest(const std::string& token, const std::string& sig_digest, const std::string& public_key);
  std::string sign_digest(const std::string& token, const std::string& sig_digest, const std::string& public_key, const std::string& wallet_name);

  // ── query ────────────────────────────────────────────────

  std::string has_matching_private_key(const std::string& token, const std::string& wallet_name, const std::string& public_key);
  std::string get_info(const std::string& token);

private:
  static constexpr const char* prefix_ = "STM";

  js_callback_storage            storage_;
  beekeeper_minimal::beekeeper   bk_;

  /// Runs fn() and wraps the result in {"result":...} / {"error":...}
  std::string wrap(std::function<std::string()> fn);
};

} // namespace beekeeper_wasm
