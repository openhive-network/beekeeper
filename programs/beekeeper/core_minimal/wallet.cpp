#include <core_minimal/wallet.hpp>

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
  auto wallet_file_content = crypto_.encrypt_wallet_data(password_, keys_);

  wallet_data_.cipher_keys = crypto_.parse_wallet_file(wallet_file_content);

  if (storage_)
    storage_->save(name_, wallet_file_content);
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
  keys_ = crypto_.decrypt_wallet_data(password, wallet_data_.cipher_keys);

  password_ = password;
  unlocked_ = true;
}

void wallet::lock()
{
  if (is_locked())
    return;

  encrypt_and_save();

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

std::string wallet::import_key(const std::string& wif_key, const std::string& prefix)
{
  if (is_locked())
    throw std::runtime_error("Wallet is locked: " + name_);

  auto priv = crypto_.wif_to_key(wif_key);
  if (!priv)
    throw std::runtime_error("Invalid WIF key");

  auto pub = crypto_.get_public_key(*priv);
  std::string pub_str = crypto_.public_key_to_string(pub, prefix);

  if (keys_.find(pub) == keys_.end())
  {
    keys_.emplace(pub, key_data(*priv, prefix));
    encrypt_and_save();
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
