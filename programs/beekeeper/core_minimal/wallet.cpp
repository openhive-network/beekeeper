#include <core_minimal/wallet.hpp>
#include <core_minimal/binary_serializer.hpp>

#include <cctype>
#include <stdexcept>

namespace beekeeper_minimal {

wallet::wallet(crypto_provider& crypto, wallet_storage* storage, std::string name)
  : crypto_(crypto), storage_(storage), name_(std::move(name))
{
}

const std::string& wallet::get_name() const { return name_; }

// ── helpers ──────────────────────────────────────────────────

namespace {

/// Wallet file format: the encrypted blob as a JSON hex string "abcd...".
std::vector<char> to_wallet_file(const std::vector<char>& cipher_keys)
{
  auto hex = hex_encode(cipher_keys);
  std::vector<char> buf;
  buf.reserve(hex.size() + 2);
  buf.push_back('"');
  buf.insert(buf.end(), hex.begin(), hex.end());
  buf.push_back('"');
  return buf;
}

/// True if a stored wallet file holds exactly this blob, read the way
/// parse_wallet_file() reads it (hex between the first and the last quote;
/// files written by older versions may differ in whitespace or hex case).
/// Reports instead of throwing, see wallet::unlock().
bool wallet_file_holds(const std::vector<char>& file, const std::vector<char>& cipher_keys)
{
  std::string json(file.begin(), file.end());
  auto start = json.find('"');
  auto end = json.rfind('"');
  if (start == std::string::npos || end == std::string::npos || start == end)
    return false;

  auto hex = hex_encode(cipher_keys);
  if (end - start - 1 != hex.size())
    return false;
  for (size_t i = 0; i < hex.size(); ++i)
    if (std::tolower(static_cast<unsigned char>(json[start + 1 + i])) != hex[i])
      return false;
  return true;
}

/// Comparison without an early exit on the first differing byte.
bool equal_secrets(const std::string& a, const std::string& b)
{
  uint8_t diff = a.size() == b.size() ? 0 : 1;
  const size_t n = a.size() > b.size() ? a.size() : b.size();
  for (size_t i = 0; i < n; ++i)
    diff |= static_cast<uint8_t>((i < a.size() ? a[i] : 0) ^ (i < b.size() ? b[i] : 0));
  return diff == 0;
}

} // anonymous namespace

void wallet::encrypt_and_save()
{
  auto cipher_keys = crypto_.encrypt_wallet_keys(key_, keys_);

  if (storage_)
    storage_->save(name_, to_wallet_file(cipher_keys));

  // Assign only after a successful save, so the in-memory blob never gets
  // ahead of the stored one when save throws.
  wallet_data_.cipher_keys = std::move(cipher_keys);
}

bool wallet::encrypt_and_save_if_unchanged(const keys_map& keys, const wallet_key& key)
{
  auto cipher_keys = crypto_.encrypt_wallet_keys(key, keys);

  if (storage_)
  {
    // Another instance sharing the storage may have rewritten the wallet since
    // it was opened here; writing this older content over it would drop those
    // changes. Storages that cannot read fresh data (the browser storage serves
    // a per-instance cache) do not detect it.
    auto stored = storage_->try_load(name_);
    if (!stored || !wallet_file_holds(*stored, wallet_data_.cipher_keys))
      return false;

    if (!storage_->try_save(name_, to_wallet_file(cipher_keys)))
      return false;
  }

  wallet_data_.cipher_keys = std::move(cipher_keys);
  return true;
}

// ── lifecycle ────────────────────────────────────────────────

void wallet::create(const std::string& password, uint32_t kdf_iterations)
{
  key_ = crypto_.derive_wallet_key(password, kdf_iterations);
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
  key_.wipe();
  unlocked_ = false;
}

void wallet::unlock(const std::string& password)
{
  if (!is_locked())
    throw std::runtime_error("Wallet is already unlocked: " + name_);

  // decrypt_wallet_data throws on bad password. For the current format it also
  // returns the derived key, which later saves reuse while the wallet is unlocked.
  wallet_key key;
  auto keys = crypto_.decrypt_wallet_data(password, wallet_data_.cipher_keys, key);

  // Transparently re-encrypt legacy (unsalted SHA-512) wallets with the
  // current format on the first successful unlock, before the unlocked state
  // is exposed. The conversion is opportunistic: when the stored wallet changed
  // meanwhile or storage cannot be written, the wallet still unlocks and the
  // stored file stays legacy until the next save or unlock. try_load/try_save
  // report failures without exceptions, which matters in the WASM build: a
  // failure there must not escape through Asyncify-suspended frames.
  if (key.iterations == 0)
  {
    key = crypto_.derive_wallet_key(password, 0);
    encrypt_and_save_if_unchanged(keys, key);
  }

  keys_ = std::move(keys);
  password_ = password;
  key_ = std::move(key);
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
  key_.wipe();
  unlocked_ = false;
}

bool wallet::is_locked() const
{
  return !unlocked_;
}

void wallet::check_password(const std::string& password) const
{
  // While unlocked, the password that decrypted the wallet is held in memory;
  // comparing against it avoids another KDF run.
  if (!is_locked())
  {
    if (!equal_secrets(password, password_))
      throw std::runtime_error("Invalid password");
    return;
  }

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
