#pragma once

#include <core_minimal/wallet.hpp>
#include <core_minimal/crypto_provider.hpp>
#include <core_minimal/memory_storage.hpp>

#include <atomic>
#include <chrono>
#include <ctime>
#include <map>
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
/// created/opened and which of those it unlocked.  A wallet that is loaded
/// once can be open in several sessions, but each session has to unlock it
/// itself (with its password) before it can use its keys; locking, closing
/// and the inactivity timeout act on the calling session only.  All
/// key/sign/encrypt/decrypt operations require an explicit wallet name.
class beekeeper
{
public:
  using clock = std::chrono::steady_clock;

  /// @param crypto             Crypto hooks.
  /// @param storage            FS hooks (save/load).
  /// @param unlock_timeout_sec Inactivity timeout of new sessions (seconds). 0 = never.
  explicit beekeeper(crypto_provider& crypto, wallet_storage& storage,
                     uint32_t unlock_timeout_sec = 900);

  // ── session management ─────────────────────────────────────

  /// Create a new session.  Returns its unique token.
  std::string create_session();

  /// Lock and close the session's wallets, then destroy the session.
  void        close_session(const std::string& token);

  // ── timeout ────────────────────────────────────────────────

  /// Change the session's inactivity timeout (0 = disable).
  void set_timeout(const std::string& token, uint32_t seconds);

  /// Call periodically to enforce auto-lock: locks the wallets of every
  /// session whose inactivity timeout has expired.
  void check_timeout();

  /// Returns current time and the session's timeout deadline as ISO8601 strings.
  session_info get_info(const std::string& token) const;

  // ── wallet queries ─────────────────────────────────────────

  /// Check if a wallet exists (in-memory or in storage).
  bool has_wallet(const std::string& wallet_name) const;

  /// List the wallets the session created or opened, and whether it unlocked them.
  std::vector<wallet_details> list_wallets(const std::string& token) const;

  // ── wallet operations ──────────────────────────────────────

  /// Create a new wallet, associated with token and unlocked in that session.
  std::string create_wallet(const std::string& token,
                            const std::string& wallet_name,
                            const std::string& password,
                            bool is_temporary = false);

  /// Open an existing wallet, associated with token.  It stays locked in this
  /// session until the session unlocks it, even if another session did.
  void open_wallet(const std::string& token, const std::string& wallet_name);

  /// Close the wallet in this session.  It is unloaded once no session has it open.
  void close_wallet(const std::string& token, const std::string& wallet_name);

  /// Lock the wallet in this session.  Its keys are wiped from memory once no
  /// session has it unlocked.
  void lock(const std::string& token, const std::string& wallet_name);
  void lock_all(const std::string& token);

  /// Unlock a wallet the session created or opened.  For a wallet another
  /// session unlocked only the password is checked; its keys in memory stay.
  void unlock(const std::string& token, const std::string& wallet_name, const std::string& password);

  // ── key operations ─────────────────────────────────────────
  // The wallet must be open and unlocked in the calling session.

  std::string import_key(const std::string& token,
                         const std::string& wallet_name,
                         const std::string& wif_key,
                         const std::string& prefix,
                         bool flush = true);


  void remove_key(const std::string& token,
                  const std::string& wallet_name,
                  const public_key_type& public_key);

  /// Get all public keys from the named wallet.
  keys_map get_public_keys(const std::string& token, const std::string& wallet_name) const;

  /// Check if a specific private key exists in the named wallet (false while
  /// the session has not unlocked it).
  bool has_private_key(const std::string& token,
                       const std::string& wallet_name,
                       const public_key_type& public_key) const;

  // ── signing ────────────────────────────────────────────────

  /// Sign a digest using a key from the named wallet.
  signature_type sign_digest(const std::string& token,
                             const std::string& wallet_name,
                             const digest_type& digest,
                             const public_key_type& public_key,
                             const std::string& prefix);

  // ── encrypt / decrypt ──────────────────────────────────────

  /// Encrypt data using ECDH. nonce=0 means auto-generate from current time.
  std::string encrypt_data(const std::string& token,
                           const std::string& wallet_name,
                           const public_key_type& from_key,
                           const public_key_type& to_key,
                           const std::string& content,
                           const std::string& prefix,
                           uint64_t nonce = 0);

  /// Decrypt data using ECDH.
  std::string decrypt_data(const std::string& token,
                           const std::string& wallet_name,
                           const public_key_type& from_key,
                           const public_key_type& to_key,
                           const std::string& encrypted_content,
                           const std::string& prefix);

  /// Validate that a session token exists. Throws if not found.
  void validate_token(const std::string& token) const;

  /// Number of currently active sessions.
  size_t session_count() const { return sessions_.size(); }

private:
  struct session_state
  {
    /// Wallets the session created or opened -> unlocked in this session.
    std::map<std::string, bool> wallets;

    std::chrono::seconds timeout{0};

    /// Auto-lock deadline (clock ticks). Atomic because signing, encrypting and
    /// decrypting refresh it, and the daemon runs those concurrently under a
    /// shared lock.
    std::atomic<clock::rep> timeout_time{clock::time_point::max().time_since_epoch().count()};
  };

  session_state& get_session(const std::string& token);
  const session_state& get_session(const std::string& token) const;

  void refresh_timeout(session_state& session);
  std::chrono::seconds get_remaining_seconds(const session_state& session) const;

  void lock_session_wallets(session_state& session);

  /// Locks a wallet no session has unlocked any more and unloads one no session
  /// has open, so that its keys do not stay in memory after the last user.
  void release_wallet(const std::string& wallet_name);

  /// The session's wallet, or nullptr while the session has not unlocked it.
  /// Throws if the session did not create or open it.
  wallet* session_wallet(const session_state& session, const std::string& wallet_name);
  const wallet* session_wallet(const session_state& session, const std::string& wallet_name) const;

  wallet& get_wallet(const std::string& wallet_name);
  const wallet& get_wallet(const std::string& wallet_name) const;

  std::string generate_token();
  std::string gen_password() const;

  crypto_provider&                         crypto_;
  wallet_storage&                          storage_;
  memory_storage                           mem_storage_;
  std::map<std::string, wallet>            wallets_;   ///< loaded wallets, shared by the sessions that opened them
  std::map<std::string, session_state>     sessions_;  ///< token -> session

  std::chrono::seconds                     default_timeout_;
};

} // namespace beekeeper_minimal
