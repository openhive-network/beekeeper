#pragma once

#include <core_minimal/wallet.hpp>

#include <chrono>
#include <map>
#include <memory>

namespace beekeeper_minimal {

/// A session owns a set of wallets and auto-locks them on timeout.
class session
{
public:
  using clock = std::chrono::steady_clock;

  /// @param token           Unique session identifier.
  /// @param unlock_timeout  Inactivity timeout (seconds) before all wallets are locked. 0 = never.
  /// @param storage         FS hooks forwarded to wallets.
  session(std::string token, uint32_t unlock_timeout_seconds, wallet_storage* storage);

  const std::string& get_token() const;

  /// Call periodically (or before each operation) to enforce auto-lock.
  void check_timeout();

  // ── wallet operations ──────────────────────────────────────

  /// Create a new wallet with the given name and password.
  /// If password is empty, one is generated and returned.
  std::string create_wallet(const std::string& wallet_name,
                            const std::string& password);

  /// Open an existing wallet from storage (locked).
  void open_wallet(const std::string& wallet_name);

  /// Close (unload) a wallet from this session.
  void close_wallet(const std::string& wallet_name);

  void lock(const std::string& wallet_name);
  void lock_all();
  void unlock(const std::string& wallet_name, const std::string& password);

  // ── key operations ─────────────────────────────────────────

  std::string import_key(const std::string& wallet_name,
                         const std::string& wif_key,
                         const std::string& prefix);

  void remove_key(const std::string& wallet_name,
                  const public_key_type& public_key);

  keys_map get_public_keys(const std::string& wallet_name) const;

  // ── signing ────────────────────────────────────────────────

  /// Sign a digest.  Searches the named wallet (or all wallets if name is empty).
  signature_type sign_digest(const std::string& wallet_name,
                             const digest_type& digest,
                             const public_key_type& public_key,
                             const std::string& prefix);

private:
  void refresh_timeout();
  wallet& get_wallet(const std::string& wallet_name);
  const wallet& get_wallet(const std::string& wallet_name) const;
  std::string gen_password() const;

  std::string                              token_;
  wallet_storage*                          storage_;
  std::map<std::string, wallet>            wallets_;

  std::chrono::seconds                     timeout_;
  clock::time_point                        timeout_time_;
};

} // namespace beekeeper_minimal
