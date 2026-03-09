#pragma once

#include <core_minimal/wallet.hpp>
#include <core_minimal/crypto_provider.hpp>
#include <core_minimal/memory_storage.hpp>

#include <chrono>
#include <ctime>
#include <map>
#include <set>
#include <vector>

namespace beekeeper_minimal {

struct wallet_details
{
  std::string name;
  bool unlocked = false;
};

struct session_info
{
  std::string now;
  std::string timeout_time;
};

/// Top-level entry point.  Manages sessions and wallets.
/// No program options, no wallet-dir, no backtrace.
/// All filesystem access is delegated through wallet_storage hooks.
/// All crypto is delegated through crypto_provider hooks.
///
/// Wallets are scoped to sessions: each session tracks which wallets it
/// created/opened.  All key/sign/encrypt/decrypt operations require an
/// explicit wallet name.
class beekeeper
{
public:
  using clock = std::chrono::steady_clock;

  /// @param crypto             Crypto hooks.
  /// @param storage            FS hooks (save/load).
  /// @param unlock_timeout_sec Default inactivity timeout (seconds). 0 = never.
  explicit beekeeper(crypto_provider& crypto, wallet_storage& storage,
                     uint32_t unlock_timeout_sec = 900);

  // ── session management ─────────────────────────────────────

  /// Create a new session.  Returns its unique token.
  std::string create_session();

  /// Destroy a session and disassociate its wallets.
  void        close_session(const std::string& token);

  // ── timeout ────────────────────────────────────────────────

  /// Change the inactivity timeout (0 = disable).
  void set_timeout(uint32_t seconds);

  /// Call periodically to enforce auto-lock.
  void check_timeout();

  /// Returns current time and timeout deadline as ISO8601 strings.
  session_info get_info() const;

  // ── wallet queries ─────────────────────────────────────────

  /// Check if a wallet exists (in-memory or in storage).
  bool has_wallet(const std::string& wallet_name) const;

  /// Validate wallet password without changing lock state.
  void check_password(const std::string& wallet_name, const std::string& password) const;

  /// List wallets belonging to token + storage-only wallets (as locked).
  std::vector<wallet_details> list_wallets(const std::string& token) const;

  // ── wallet operations ──────────────────────────────────────

  /// Create a new wallet, associated with token.
  std::string create_wallet(const std::string& token,
                            const std::string& wallet_name,
                            const std::string& password,
                            bool is_temporary = false);

  /// Open an existing wallet from storage, associated with token.
  void open_wallet(const std::string& token, const std::string& wallet_name);

  /// Close (unload) a wallet. Removes from all sessions.
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

  /// Get all public keys from the named wallet.
  keys_map get_public_keys(const std::string& wallet_name) const;

  /// Check if a specific private key exists in the named wallet.
  bool has_private_key(const std::string& wallet_name,
                       const public_key_type& public_key) const;

  // ── signing ────────────────────────────────────────────────

  /// Sign a digest using a key from the named wallet.
  signature_type sign_digest(const std::string& wallet_name,
                             const digest_type& digest,
                             const public_key_type& public_key,
                             const std::string& prefix);

  // ── encrypt / decrypt ──────────────────────────────────────

  /// Encrypt data using ECDH. nonce=0 means auto-generate from current time.
  std::string encrypt_data(const std::string& wallet_name,
                           const public_key_type& from_key,
                           const public_key_type& to_key,
                           const std::string& content,
                           const std::string& prefix,
                           uint64_t nonce = 0);

  /// Decrypt data using ECDH.
  std::string decrypt_data(const std::string& wallet_name,
                           const public_key_type& from_key,
                           const public_key_type& to_key,
                           const std::string& encrypted_content,
                           const std::string& prefix);

  /// Validate that a session token exists. Throws if not found.
  void validate_token(const std::string& token) const;

  /// Number of currently active sessions.
  size_t session_count() const { return tokens_.size(); }

private:
  void refresh_timeout();
  std::chrono::seconds get_remaining_seconds() const;

  wallet& get_wallet(const std::string& wallet_name);
  const wallet& get_wallet(const std::string& wallet_name) const;

  std::string generate_token();
  std::string gen_password() const;

  crypto_provider&                         crypto_;
  wallet_storage&                          storage_;
  memory_storage                           mem_storage_;
  std::set<std::string>                    tokens_;
  std::map<std::string, wallet>            wallets_;
  std::map<std::string, std::set<std::string>> token_wallets_;  ///< token → wallet names

  std::chrono::seconds                     timeout_;
  clock::time_point                        timeout_time_;
};

} // namespace beekeeper_minimal
