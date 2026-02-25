#include <core_minimal/wallet.hpp>

#include <fc/io/json.hpp>

namespace beekeeper_minimal {

wallet::wallet(wallet_storage* storage, std::string name)
  : storage_(storage), name_(std::move(name))
{
}

const std::string& wallet::get_name() const { return name_; }

// ── helpers ──────────────────────────────────────────────────

bool wallet::is_checksum_valid(const fc::sha512& pw, const std::vector<char>& decrypted) const
{
  fc::sha512 stored_checksum;
  fc::raw::unpack_from_vector<fc::sha512>(decrypted, stored_checksum);
  return pw == stored_checksum;
}

std::vector<char> wallet::decrypt_with(const std::string& password) const
{
  auto pw = fc::sha512::hash(password.c_str(), password.size());

  std::vector<char> decrypted;
  try
  {
    decrypted = fc::aes_decrypt(pw, wallet_data_.cipher_keys);
    if (!is_checksum_valid(pw, decrypted))
      throw std::runtime_error("checksum mismatch");
  }
  catch (...)
  {
    throw std::runtime_error("Invalid password for wallet: '" + name_ + "'");
  }

  return decrypted;
}

void wallet::encrypt_and_save()
{
  plain_keys data;
  data.keys     = keys_;
  data.checksum = checksum_;
  auto plain_txt = fc::raw::pack_to_vector(data);
  wallet_data_.cipher_keys = fc::aes_encrypt(data.checksum, plain_txt);

  if (storage_)
  {
    std::string json = fc::json::to_pretty_string(wallet_data_);
    std::vector<char> buf(json.begin(), json.end());
    storage_->save(name_, buf);
  }
}

// ── lifecycle ────────────────────────────────────────────────

void wallet::create(const std::string& password)
{
  checksum_ = fc::sha512::hash(password.c_str(), password.size());
  // Start with empty keys, encrypt, save, then remain unlocked
  keys_.clear();
  encrypt_and_save();
  // wallet is now unlocked (checksum_ is set, keys_ is accessible)
}

void wallet::open()
{
  if (!storage_)
    throw std::runtime_error("Cannot open wallet without storage");

  auto buf = storage_->load(name_);
  std::string json(buf.begin(), buf.end());
  wallet_data_ = fc::json::from_string(json, fc::json::format_validation_mode::full).as<wallet_data>();
  // Remains locked until unlock() is called
  keys_.clear();
  checksum_ = fc::sha512();
}

void wallet::unlock(const std::string& password)
{
  if (!is_locked())
    throw std::runtime_error("Wallet is already unlocked: " + name_);

  auto decrypted = decrypt_with(password);

  plain_keys pk;
  fc::raw::unpack_from_vector<plain_keys>(decrypted, pk, 0, true);

  keys_     = std::move(pk.keys);
  checksum_ = pk.checksum;
}

void wallet::lock()
{
  if (is_locked())
    return;

  encrypt_and_save();

  for (auto& kv : keys_)
    kv.second = key_data(private_key_type(), "");

  keys_.clear();
  checksum_ = fc::sha512();
}

bool wallet::is_locked() const
{
  return checksum_ == fc::sha512();
}

void wallet::check_password(const std::string& password) const
{
  decrypt_with(password); // throws on bad password
}

// ── key management ───────────────────────────────────────────

std::string wallet::import_key(const std::string& wif_key, const std::string& prefix)
{
  if (is_locked())
    throw std::runtime_error("Wallet is locked: " + name_);

  auto priv = private_key_type::wif_to_key(wif_key);
  if (!priv.valid())
    throw std::runtime_error("Invalid WIF key");

  auto pub = priv->get_public_key();
  std::string pub_str = public_key_to_string(pub, prefix);

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

  return it->second.first.sign_compact(digest);
}

} // namespace beekeeper_minimal
