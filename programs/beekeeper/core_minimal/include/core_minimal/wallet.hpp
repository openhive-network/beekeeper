#pragma once

#include <core_minimal/types.hpp>
#include <core_minimal/wallet_storage.hpp>

#include <memory>

namespace beekeeper_minimal {

/// A single wallet: password-based AES encryption of private keys.
/// All persistence is delegated to the wallet_storage hooks.
class wallet
{
public:
  /// @param storage  FS hooks (save/load). nullptr for in-memory-only wallets.
  /// @param name     Logical wallet name (used as storage path/identifier).
  explicit wallet(wallet_storage* storage = nullptr, std::string name = "");

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
  std::string import_key(const std::string& wif_key, const std::string& prefix);

  /// Remove a key by its public key.  Persists to storage automatically.
  void        remove_key(const public_key_type& public_key);

  /// All public/private key pairs currently in this wallet.
  const keys_map& get_keys() const;

  bool has_private_key(const public_key_type& public_key) const;

  // ── signing ────────────────────────────────────────────────

  /// Sign a digest with the private key that corresponds to the given public key.
  /// Returns nullopt if this wallet doesn't contain the key.
  std::optional<signature_type> try_sign_digest(const digest_type& digest,
                                                const public_key_type& public_key) const;

private:
  void encrypt_and_save();
  std::vector<char> decrypt_with(const std::string& password) const;
  bool is_checksum_valid(const fc::sha512& pw, const std::vector<char>& decrypted) const;

  wallet_storage*    storage_;
  std::string        name_;

  wallet_data        wallet_data_;   // encrypted blob
  keys_map           keys_;          // plaintext keys (empty when locked)
  fc::sha512         checksum_;      // sha512(password), zero when locked
};

} // namespace beekeeper_minimal
