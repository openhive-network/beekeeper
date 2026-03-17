#pragma once

#include <core_minimal/types.hpp>
#include <core_minimal/wallet_storage.hpp>
#include <core_minimal/crypto_provider.hpp>

#include <memory>

namespace beekeeper_minimal {

/// A single wallet: password-based AES encryption of private keys.
/// All persistence is delegated to wallet_storage hooks.
/// All crypto is delegated to crypto_provider hooks.
class wallet
{
public:
  /// @param crypto   Crypto hooks (encrypt, decrypt, sign, etc.).
  /// @param storage  FS hooks (save/load). nullptr for in-memory-only wallets.
  /// @param name     Logical wallet name (used as storage path/identifier).
  explicit wallet(crypto_provider& crypto,
                  wallet_storage* storage = nullptr,
                  std::string name = "");

  const std::string& get_name() const;

  // ── lifecycle ──────────────────────────────────────────────

  /// Initialise a brand-new wallet with a password.  Leaves the wallet unlocked.
  void     create(const std::string& password);

  /// Load cipher_keys from storage, then remain locked until unlock() is called.
  void     open();

  /// Decrypt keys with the given password.
  void     unlock(const std::string& password);

  /// Encrypt keys, wipe in-memory plaintext.
  void     lock();

  bool     is_locked() const;

  /// Validate the password without changing the lock state.
  void     check_password(const std::string& password) const;

  // ── key management (wallet must be unlocked) ───────────────

  /// Import a WIF-encoded private key. Returns the public key string (with prefix).
  /// Persists to storage automatically.
  std::string import_key(const std::string& wif_key, const std::string& prefix, bool flush = true);

  /// Remove a key by its public key.  Persists to storage automatically.
  void        remove_key(const public_key_type& public_key);

  /// All public/private key pairs currently in this wallet.
  const keys_map& get_keys() const;

  bool has_private_key(const public_key_type& public_key) const;

  /// Look up the private key that corresponds to the given public key.
  /// Returns nullopt if locked or key not found.
  std::optional<private_key_type> find_private_key(const public_key_type& public_key) const;

  // ── signing ────────────────────────────────────────────────

  /// Sign a digest with the private key that corresponds to the given public key.
  /// Returns nullopt if this wallet doesn't contain the key.
  std::optional<signature_type> try_sign_digest(const digest_type& digest,
                                                const public_key_type& public_key) const;

private:
  void encrypt_and_save();

  crypto_provider&   crypto_;
  wallet_storage*    storage_;
  std::string        name_;

  wallet_data        wallet_data_;   // encrypted blob
  keys_map           keys_;          // plaintext keys (empty when locked)
  std::string        password_;      // stored while unlocked, cleared on lock
  bool               unlocked_{false};
  bool               dirty_{false};    // true when keys_ changed but not yet saved
};

} // namespace beekeeper_minimal
