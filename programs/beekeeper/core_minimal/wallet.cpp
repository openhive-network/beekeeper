#include <core_minimal/wallet.hpp>
#include <core_minimal/binary_serializer.hpp>

#include <stdexcept>

namespace beekeeper_minimal {

wallet::wallet(crypto_provider& crypto, wallet_storage* storage, std::string name)
  : crypto_(crypto), storage_(storage), name_(std::move(name))
{
}

const std::string& wallet::get_name() const { return name_; }

// ── helpers ──────────────────────────────────────────────────

void wallet::encrypt_and_save()
{
  encrypt_and_save(password_, keys_);
}

void wallet::encrypt_and_save(const std::string& password, const keys_map& keys)
{
  auto cipher_keys = crypto_.encrypt_wallet_keys(password, keys);

  if (storage_)
  {
    // Serialize cipher_keys as the wallet file format: JSON hex string "abcd..."
    auto hex = hex_encode(cipher_keys);
    std::vector<char> buf;
    buf.reserve(hex.size() + 2);
    buf.push_back('"');
    buf.insert(buf.end(), hex.begin(), hex.end());
    buf.push_back('"');
    storage_->save(name_, buf);
  }

  // Assign only after a successful save, so the in-memory blob never gets
  // ahead of the stored one when save throws.
  wallet_data_.cipher_keys = std::move(cipher_keys);
}

// ── lifecycle ────────────────────────────────────────────────

void wallet::create(const std::string& password)
{
  password_ = password;
  unlocked_ = true;
  keys_.clear();
  encrypt_and_save();
}

void wallet::open()
{
  if (!storage_)
    throw std::runtime_error("Cannot open wallet without storage");

  auto buf = storage_->load(name_);
  wallet_data_.cipher_keys = crypto_.parse_wallet_file(buf);

  keys_.clear();
  password_.clear();
  unlocked_ = false;
}

void wallet::unlock(const std::string& password)
{
  if (!is_locked())
    throw std::runtime_error("Wallet is already unlocked: " + name_);

  // decrypt_wallet_data throws on bad password
  auto keys = crypto_.decrypt_wallet_data(password, wallet_data_.cipher_keys);

  // Transparently re-encrypt legacy (unsalted SHA-512) wallets with the
  // current format on the first successful unlock. This happens before the
  // unlocked state is exposed: an error that escapes (in the WASM build a JS
  // storage exception bypasses C++ catch blocks) then leaves the wallet
  // consistently locked instead of half-unlocked. Migration stays
  // opportunistic - with unavailable storage (e.g. read-only wallet dir) the
  // wallet still unlocks and the file stays legacy; a future unlock retries.
  if (crypto_.is_legacy_wallet(wallet_data_.cipher_keys))
  {
    try
    {
      encrypt_and_save(password, keys);
    }
    catch (...)
    {
    }
  }

  keys_ = std::move(keys);
  password_ = password;
  unlocked_ = true;
}

void wallet::lock()
{
  if (is_locked())
    return;

  for (auto& kv : keys_)
    kv.second = key_data(private_key_type(), "");

  keys_.clear();
  password_.clear();
  unlocked_ = false;
}

bool wallet::is_locked() const
{
  return !unlocked_;
}

void wallet::check_password(const std::string& password) const
{
  crypto_.validate_password(password, wallet_data_.cipher_keys);
}

// ── key management ───────────────────────────────────────────

std::string wallet::import_key(const std::string& wif_key, const std::string& prefix, bool flush)
{
  if (is_locked())
    throw std::runtime_error("Wallet is locked: " + name_);

  auto priv = crypto_.wif_to_key(wif_key);
  if (!priv)
    throw std::runtime_error("Invalid WIF key");

  auto pub = crypto_.get_public_key(*priv);
  std::string pub_str = crypto_.public_key_to_string(pub, prefix);

  bool inserted = false;
  if (keys_.find(pub) == keys_.end())
  {
    keys_.emplace(pub, key_data(*priv, prefix));
    inserted = true;
    dirty_ = true;
  }

  if (flush && dirty_)
  {
    encrypt_and_save();
    dirty_ = false;
  }
  return pub_str;
}

void wallet::remove_key(const public_key_type& public_key)
{
  if (is_locked())
    throw std::runtime_error("Wallet is locked: " + name_);

  auto it = keys_.find(public_key);
  if (it == keys_.end())
    throw std::runtime_error("Key not in wallet");

  keys_.erase(it);
  encrypt_and_save();
}

const keys_map& wallet::get_keys() const
{
  if (is_locked())
    throw std::runtime_error("Wallet is locked: " + name_);
  return keys_;
}

bool wallet::has_private_key(const public_key_type& public_key) const
{
  if (is_locked())
    return false;
  return keys_.find(public_key) != keys_.end();
}

std::optional<private_key_type> wallet::find_private_key(const public_key_type& public_key) const
{
  if (is_locked())
    return std::nullopt;
  auto it = keys_.find(public_key);
  if (it == keys_.end())
    return std::nullopt;
  return it->second.first;
}

// ── signing ──────────────────────────────────────────────────

std::optional<signature_type> wallet::try_sign_digest(const digest_type& digest,
                                                      const public_key_type& public_key) const
{
  if (is_locked())
    return std::nullopt;

  auto it = keys_.find(public_key);
  if (it == keys_.end())
    return std::nullopt;

  return crypto_.sign_compact(it->second.first, digest);
}

} // namespace beekeeper_minimal
