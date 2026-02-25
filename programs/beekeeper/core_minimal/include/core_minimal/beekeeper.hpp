#pragma once

#include <core_minimal/session.hpp>
#include <core_minimal/crypto_provider.hpp>

#include <map>
#include <memory>

namespace beekeeper_minimal {

/// Top-level entry point.  Manages sessions.
/// No program options, no wallet-dir, no backtrace.
/// All filesystem access is delegated through wallet_storage hooks.
/// All crypto is delegated through crypto_provider hooks.
class beekeeper
{
public:
  /// @param crypto             Crypto hooks.
  /// @param storage            FS hooks (save/load).
  /// @param unlock_timeout_sec Default inactivity timeout for new sessions (seconds). 0 = never.
  explicit beekeeper(crypto_provider& crypto, wallet_storage& storage,
                     uint32_t unlock_timeout_sec = 900);

  // ── session management ─────────────────────────────────────

  /// Create a new session.  Returns its unique token.
  std::string create_session(const std::string& salt = "");

  /// Destroy a session and all its loaded wallets.
  void        close_session(const std::string& token);

  /// Get a session by its token.
  session&    get_session(const std::string& token);

  /// Tick all sessions' timeouts (call from your event loop / timer).
  void        check_timeouts();

private:
  std::string generate_token(const std::string& salt) const;

  crypto_provider&                         crypto_;
  wallet_storage&                          storage_;
  uint32_t                                 unlock_timeout_sec_;
  std::map<std::string, session>           sessions_;
};

} // namespace beekeeper_minimal
