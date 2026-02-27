#pragma once

#include <core_minimal/wallet.hpp>
#include <core_minimal/crypto_provider.hpp>
#include <core_minimal/memory_storage.hpp>

#include <chrono>
#include <map>
#include <memory>
#include <vector>

namespace beekeeper_minimal {

struct wallet_details
{
  std::string name;
  bool unlocked = false;
};

/// A session owns a set of wallets and auto-locks them on timeout.
class session
{
public:
  using clock = std::chrono::steady_clock;

  /// @param token           Unique session identifier.
  /// @param unlock_timeout  Inactivity timeout (seconds) before all wallets are locked. 0 = never.
  /// @param crypto          Crypto provider hooks.
  /// @param storage         FS hooks forwarded to wallets.
  session(std::string token, uint32_t unlock_timeout_seconds,
          crypto_provider& crypto, wallet_storage* storage);

  const std::string& get_token() const;

  /// Call periodically (or before each operation) to enforce auto-lock.
  void check_timeout();

  /// Returns seconds until auto-lock, or seconds::max() if timeout is disabled.
  std::chrono::seconds get_remaining_seconds() const;

  // ── wallet queries ────────────────────────────────────────

  /// Check if a wallet exists (in-memory or in storage).
  bool has_wallet(const std::string& wallet_name) const;

  /// List wallets: merges in-memory (with unlock status) + storage-only (as locked).
  std::vector<wallet_details> list_wallets() const;

  // ── wallet operations ──────────────────────────────────────

  /// Create a new wallet with the given name and password.
  /// If password is empty, one is generated and returned.
  /// @param is_temporary  When true, the wallet uses an in-memory store
  ///                      (bypasses the session's persistent storage callbacks).
  std::string create_wallet(const std::string& wallet_name,
                            const std::string& password,
                            bool is_temporary = false);

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

  // ── encrypt / decrypt ───────────────────────────────────────

  /// Encrypt data using ECDH between from_key and to_key.
  /// Searches the named wallet (or all wallets if name is empty) for from_key's private key.
  std::string encrypt_data(const std::string& wallet_name,
                           const public_key_type& from_key,
                           const public_key_type& to_key,
                           const std::string& content,
                           const std::string& prefix,
                           std::optional<uint64_t> nonce = {});

  /// Decrypt data using ECDH between from_key and to_key.
  /// Searches the named wallet (or all wallets if name is empty) for the matching private key.
  std::string decrypt_data(const std::string& wallet_name,
                           const public_key_type& from_key,
                           const public_key_type& to_key,
                           const std::string& encrypted_content,
                           const std::string& prefix);

private:
  void refresh_timeout();
  wallet& get_wallet(const std::string& wallet_name);
  const wallet& get_wallet(const std::string& wallet_name) const;
  std::string gen_password() const;

  std::string                              token_;
  crypto_provider&                         crypto_;
  wallet_storage*                          storage_;
  memory_storage                           mem_storage_;
  std::map<std::string, wallet>            wallets_;

  std::chrono::seconds                     timeout_;
  clock::time_point                        timeout_time_;
};

} // namespace beekeeper_minimal
