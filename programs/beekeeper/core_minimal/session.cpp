#include <core_minimal/session.hpp>

#include <algorithm>
#include <stdexcept>

namespace beekeeper_minimal {

session::session(std::string token, uint32_t unlock_timeout_seconds,
                 crypto_provider& crypto, wallet_storage* storage)
  : token_(std::move(token))
  , crypto_(crypto)
  , storage_(storage)
  , timeout_(unlock_timeout_seconds)
  , timeout_time_(unlock_timeout_seconds > 0
                    ? clock::now() + std::chrono::seconds(unlock_timeout_seconds)
                    : clock::time_point::max())
{
}

const std::string& session::get_token() const { return token_; }

void session::refresh_timeout()
{
  if (timeout_.count() > 0)
    timeout_time_ = clock::now() + timeout_;
}

void session::check_timeout()
{
  if (timeout_.count() > 0 && clock::now() >= timeout_time_)
    lock_all();
}

std::string session::gen_password() const
{
  auto key = crypto_.generate_private_key();
  return "PW" + crypto_.key_to_wif(key);
}

wallet& session::get_wallet(const std::string& wallet_name)
{
  auto it = wallets_.find(wallet_name);
  if (it == wallets_.end())
    throw std::runtime_error("Wallet not found: " + wallet_name);
  return it->second;
}

const wallet& session::get_wallet(const std::string& wallet_name) const
{
  auto it = wallets_.find(wallet_name);
  if (it == wallets_.end())
    throw std::runtime_error("Wallet not found: " + wallet_name);
  return it->second;
}

// ── wallet queries ──────────────────────────────────────────

bool session::has_wallet(const std::string& wallet_name) const
{
  if (wallets_.count(wallet_name))
    return true;

  if (storage_)
  {
    auto names = storage_->list_dir();
    return std::find(names.begin(), names.end(), wallet_name) != names.end();
  }

  return false;
}

std::vector<wallet_details> session::list_wallets() const
{
  std::vector<wallet_details> result;

  // Add in-memory wallets with their unlock status
  for (const auto& [name, w] : wallets_)
    result.push_back({ name, !w.is_locked() });

  // Merge storage-only wallets (not yet opened in this session)
  if (storage_)
  {
    auto names = storage_->list_dir();
    for (auto& name : names)
    {
      if (!wallets_.count(name))
        result.push_back({ name, false });
    }
  }

  return result;
}

// ── wallet operations ────────────────────────────────────────

std::string session::create_wallet(const std::string& wallet_name,
                                   const std::string& password)
{
  refresh_timeout();

  if (wallets_.count(wallet_name))
    throw std::runtime_error("Wallet already exists in session: " + wallet_name);

  std::string pw = password.empty() ? gen_password() : password;

  auto [it, _] = wallets_.emplace(wallet_name, wallet(crypto_, storage_, wallet_name));
  it->second.create(pw);

  return pw;
}

void session::open_wallet(const std::string& wallet_name)
{
  refresh_timeout();

  if (wallets_.count(wallet_name))
    return; // already loaded

  auto [it, _] = wallets_.emplace(wallet_name, wallet(crypto_, storage_, wallet_name));
  it->second.open(); // loads from storage, remains locked
}

void session::close_wallet(const std::string& wallet_name)
{
  refresh_timeout();
  wallets_.erase(wallet_name);
}

void session::lock(const std::string& wallet_name)
{
  refresh_timeout();
  get_wallet(wallet_name).lock();
}

void session::lock_all()
{
  for (auto& [name, w] : wallets_)
  {
    if (!w.is_locked())
      w.lock();
  }
}

void session::unlock(const std::string& wallet_name, const std::string& password)
{
  refresh_timeout();

  // Auto-open if not yet loaded
  if (!wallets_.count(wallet_name))
    open_wallet(wallet_name);

  get_wallet(wallet_name).unlock(password);
}

// ── key operations ───────────────────────────────────────────

std::string session::import_key(const std::string& wallet_name,
                                const std::string& wif_key,
                                const std::string& prefix)
{
  refresh_timeout();
  return get_wallet(wallet_name).import_key(wif_key, prefix);
}

void session::remove_key(const std::string& wallet_name,
                         const public_key_type& public_key)
{
  refresh_timeout();
  get_wallet(wallet_name).remove_key(public_key);
}

keys_map session::get_public_keys(const std::string& wallet_name) const
{
  if (!wallet_name.empty())
    return get_wallet(wallet_name).get_keys();

  // Merge keys from all unlocked wallets
  keys_map result;
  for (auto& [name, w] : wallets_)
  {
    if (!w.is_locked())
    {
      auto& k = w.get_keys();
      result.insert(k.begin(), k.end());
    }
  }
  return result;
}

// ── signing ──────────────────────────────────────────────────

signature_type session::sign_digest(const std::string& wallet_name,
                                    const digest_type& digest,
                                    const public_key_type& public_key,
                                    const std::string& prefix)
{
  refresh_timeout();

  if (!wallet_name.empty())
  {
    auto sig = get_wallet(wallet_name).try_sign_digest(digest, public_key);
    if (sig)
      return *sig;
    throw std::runtime_error("Public key " + crypto_.public_key_to_string(public_key, prefix) +
                             " not found in wallet " + wallet_name);
  }

  // Search all unlocked wallets
  for (auto& [name, w] : wallets_)
  {
    auto sig = w.try_sign_digest(digest, public_key);
    if (sig)
      return *sig;
  }

  throw std::runtime_error("Public key " + crypto_.public_key_to_string(public_key, prefix) +
                           " not found in any unlocked wallet");
}

// ── encrypt / decrypt ────────────────────────────────────────

std::string session::encrypt_data(const std::string& wallet_name,
                                  const public_key_type& from_key,
                                  const public_key_type& to_key,
                                  const std::string& content,
                                  const std::string& prefix,
                                  std::optional<uint64_t> nonce)
{
  refresh_timeout();

  // Find from_key's private key in the named wallet (or all wallets)
  std::optional<private_key_type> priv;
  if (!wallet_name.empty())
  {
    priv = get_wallet(wallet_name).find_private_key(from_key);
  }
  else
  {
    for (auto& [name, w] : wallets_)
    {
      priv = w.find_private_key(from_key);
      if (priv)
        break;
    }
  }

  if (!priv)
    throw std::runtime_error("Public key " + crypto_.public_key_to_string(from_key, prefix) +
                             " not found in " + (wallet_name.empty() ? "any unlocked wallet" : ("wallet " + wallet_name)));

  return crypto_.ecdh_encrypt(*priv, to_key, content, nonce);
}

std::string session::decrypt_data(const std::string& wallet_name,
                                  const public_key_type& from_key,
                                  const public_key_type& to_key,
                                  const std::string& encrypted_content,
                                  const std::string& prefix)
{
  refresh_timeout();

  // Build a key_finder that searches the named wallet (or all wallets)
  crypto_provider::key_finder_type key_finder;
  if (!wallet_name.empty())
  {
    auto& w = get_wallet(wallet_name);
    key_finder = [&w](const public_key_type& pk) -> std::optional<private_key_type> {
      return w.find_private_key(pk);
    };
  }
  else
  {
    key_finder = [this](const public_key_type& pk) -> std::optional<private_key_type> {
      for (auto& [name, w] : wallets_)
      {
        auto priv = w.find_private_key(pk);
        if (priv)
          return priv;
      }
      return std::nullopt;
    };
  }

  return crypto_.ecdh_decrypt(key_finder, from_key, to_key, encrypted_content);
}

} // namespace beekeeper_minimal
