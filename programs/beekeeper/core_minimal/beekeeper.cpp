#include <core_minimal/beekeeper.hpp>

#include <ctime>
#include <stdexcept>

namespace {

// Manual ISO-8601 formatting — avoids pulling in strftime → snprintf → printf_core → fmt_fp (~6 KB in WASM).
void format_iso8601(char* buf, std::time_t t)
{
  auto* tm = std::gmtime(&t);
  auto write2 = [](char* p, int v) { p[0] = '0' + v / 10; p[1] = '0' + v % 10; };
  int y = tm->tm_year + 1900;
  buf[0] = '0' + y / 1000; buf[1] = '0' + (y / 100) % 10;
  buf[2] = '0' + (y / 10) % 10; buf[3] = '0' + y % 10;
  buf[4] = '-'; write2(buf + 5, tm->tm_mon + 1);
  buf[7] = '-'; write2(buf + 8, tm->tm_mday);
  buf[10] = 'T'; write2(buf + 11, tm->tm_hour);
  buf[13] = ':'; write2(buf + 14, tm->tm_min);
  buf[16] = ':'; write2(buf + 17, tm->tm_sec);
  buf[19] = '\0';
}

} // anonymous namespace

namespace beekeeper_minimal {

beekeeper::beekeeper(crypto_provider& crypto, wallet_storage& storage,
                     uint32_t unlock_timeout_sec)
  : crypto_(crypto)
  , storage_(storage)
  , timeout_(unlock_timeout_sec)
  , timeout_time_(unlock_timeout_sec > 0
                    ? clock::now() + std::chrono::seconds(unlock_timeout_sec)
                    : clock::time_point::max())
{
}

// ── session management ──────────────────────────────────────

std::string beekeeper::generate_token()
{
  static constexpr char hex_chars[] = "0123456789abcdef";
  constexpr unsigned int token_length = 32;

  uint8_t buf[token_length];
  crypto_.get_random_bytes(buf, token_length);

  std::string result;
  result.reserve(token_length * 2);
  for (unsigned int i = 0; i < token_length; ++i)
  {
    result += hex_chars[buf[i] >> 4];
    result += hex_chars[buf[i] & 0x0f];
  }
  return result;
}

std::string beekeeper::create_session()
{
  std::string token = generate_token();

  while (token_wallets_.count(token))
    token = generate_token();

  token_wallets_[token];  // create empty set
  return token;
}

void beekeeper::close_session(const std::string& token)
{
  token_wallets_.erase(token);
}

void beekeeper::validate_token(const std::string& token) const
{
  if (!token_wallets_.count(token))
    throw std::runtime_error("Session not found: " + token);
}

// ── timeout ─────────────────────────────────────────────────

void beekeeper::set_timeout(uint32_t seconds)
{
  timeout_ = std::chrono::seconds(seconds);
  refresh_timeout();
}

void beekeeper::refresh_timeout()
{
  if (timeout_.count() > 0)
    timeout_time_ = clock::now() + timeout_;
}

void beekeeper::check_timeout()
{
  if (timeout_.count() > 0 && clock::now() >= timeout_time_)
    lock_all();
}

std::chrono::seconds beekeeper::get_remaining_seconds() const
{
  if (timeout_.count() == 0)
    return std::chrono::seconds::max();

  auto remaining = timeout_time_ - clock::now();
  if (remaining.count() <= 0)
    return std::chrono::seconds(0);

  return std::chrono::duration_cast<std::chrono::seconds>(remaining);
}

session_info beekeeper::get_info() const
{
  auto now = std::chrono::system_clock::now();
  auto now_t = std::chrono::system_clock::to_time_t(now);
  char buf[32];
  format_iso8601(buf, now_t);

  auto remaining = get_remaining_seconds();
  std::string timeout_str;
  if (remaining == std::chrono::seconds::max())
  {
    timeout_str = "9999-12-31T23:59:59";
  }
  else
  {
    auto timeout_tp = now + remaining;
    auto timeout_t = std::chrono::system_clock::to_time_t(timeout_tp);
    char tbuf[32];
    format_iso8601(tbuf, timeout_t);
    timeout_str = std::string(tbuf);
  }

  return session_info{ std::string(buf), std::move(timeout_str) };
}

// ── wallet helpers ──────────────────────────────────────────

std::string beekeeper::gen_password() const
{
  auto key = crypto_.generate_private_key();
  return "PW" + crypto_.key_to_wif(key);
}

wallet& beekeeper::get_wallet(const std::string& wallet_name)
{
  auto it = wallets_.find(wallet_name);
  if (it == wallets_.end())
    throw std::runtime_error("Wallet not found: " + wallet_name);
  return it->second;
}

const wallet& beekeeper::get_wallet(const std::string& wallet_name) const
{
  auto it = wallets_.find(wallet_name);
  if (it == wallets_.end())
    throw std::runtime_error("Wallet not found: " + wallet_name);
  return it->second;
}

// ── wallet queries ──────────────────────────────────────────

bool beekeeper::has_wallet(const std::string& wallet_name) const
{
  if (wallets_.count(wallet_name))
    return true;

  return storage_.scan_dir(wallet_name);
}

void beekeeper::check_password(const std::string& wallet_name, const std::string& password) const
{
  get_wallet(wallet_name).check_password(password);
}

std::vector<wallet_details> beekeeper::list_wallets(const std::string& token) const
{
  std::vector<wallet_details> result;

  auto it = token_wallets_.find(token);
  if (it != token_wallets_.end())
  {
    for (auto& name : it->second)
    {
      auto wit = wallets_.find(name);
      if (wit != wallets_.end())
        result.push_back({ name, !wit->second.is_locked() });
    }
  }

  return result;
}

// ── wallet operations ───────────────────────────────────────

std::string beekeeper::create_wallet(const std::string& token,
                                     const std::string& wallet_name,
                                     const std::string& password,
                                     bool is_temporary)
{
  refresh_timeout();

  if (wallets_.count(wallet_name))
    throw std::runtime_error("Wallet already exists: " + wallet_name);

  std::string pw = password.empty() ? gen_password() : password;

  wallet_storage* ws = is_temporary ? &mem_storage_ : &storage_;
  auto [it, _] = wallets_.emplace(wallet_name, wallet(crypto_, ws, wallet_name));
  it->second.create(pw);

  token_wallets_[token].insert(wallet_name);

  return pw;
}

void beekeeper::open_wallet(const std::string& token, const std::string& wallet_name)
{
  refresh_timeout();

  if (!wallets_.count(wallet_name))
  {
    auto [it, inserted] = wallets_.emplace(wallet_name, wallet(crypto_, &storage_, wallet_name));
    try
    {
      it->second.open();
    }
    catch(...)
    {
      // Remove the partially-constructed entry so has_wallet() does not
      // report a wallet whose file could not be loaded (e.g. deleted from disk).
      // Without this cleanup the subsequent create() call would be rejected
      // with "already exists" and wallet recovery would fail.
      wallets_.erase(it);
      throw;
    }
  }

  token_wallets_[token].insert(wallet_name);
}

void beekeeper::close_wallet(const std::string& wallet_name)
{
  refresh_timeout();
  wallets_.erase(wallet_name);

  // Remove from all sessions
  for (auto& [tok, names] : token_wallets_)
    names.erase(wallet_name);
}

void beekeeper::lock(const std::string& wallet_name)
{
  refresh_timeout();
  get_wallet(wallet_name).lock();
}

void beekeeper::lock_all()
{
  for (auto& [name, w] : wallets_)
  {
    if (!w.is_locked())
      w.lock();
  }
}

void beekeeper::unlock(const std::string& wallet_name, const std::string& password)
{
  refresh_timeout();

  if (!wallets_.count(wallet_name))
    throw std::runtime_error("Wallet not found: " + wallet_name);

  get_wallet(wallet_name).unlock(password);
}

// ── key operations ──────────────────────────────────────────

std::string beekeeper::import_key(const std::string& wallet_name,
                                  const std::string& wif_key,
                                  const std::string& prefix,
                                  bool flush)
{
  refresh_timeout();
  return get_wallet(wallet_name).import_key(wif_key, prefix, flush);
}


void beekeeper::remove_key(const std::string& wallet_name,
                           const public_key_type& public_key)
{
  refresh_timeout();
  get_wallet(wallet_name).remove_key(public_key);
}

keys_map beekeeper::get_public_keys(const std::string& wallet_name) const
{
  return get_wallet(wallet_name).get_keys();
}

bool beekeeper::has_private_key(const std::string& wallet_name,
                                const public_key_type& public_key) const
{
  return get_wallet(wallet_name).has_private_key(public_key);
}

// ── signing ─────────────────────────────────────────────────

signature_type beekeeper::sign_digest(const std::string& wallet_name,
                                      const digest_type& digest,
                                      const public_key_type& public_key,
                                      const std::string& prefix)
{
  refresh_timeout();

  auto sig = get_wallet(wallet_name).try_sign_digest(digest, public_key);
  if (sig)
    return *sig;
  throw std::runtime_error("Public key " + crypto_.public_key_to_string(public_key, prefix) +
                           " not found in wallet " + wallet_name);
}

// ── encrypt / decrypt ───────────────────────────────────────

std::string beekeeper::encrypt_data(const std::string& wallet_name,
                                    const public_key_type& from_key,
                                    const public_key_type& to_key,
                                    const std::string& content,
                                    const std::string& prefix,
                                    uint64_t nonce)
{
  refresh_timeout();

  auto priv = get_wallet(wallet_name).find_private_key(from_key);
  if (!priv)
    throw std::runtime_error("Public key " + crypto_.public_key_to_string(from_key, prefix) +
                             " not found in wallet " + wallet_name);

  std::optional<uint64_t> nonce_opt;
  if (nonce != 0)
    nonce_opt = nonce;

  return crypto_.ecdh_encrypt(*priv, to_key, content, nonce_opt);
}

std::string beekeeper::decrypt_data(const std::string& wallet_name,
                                    const public_key_type& from_key,
                                    const public_key_type& to_key,
                                    const std::string& encrypted_content,
                                    const std::string& prefix)
{
  refresh_timeout();

  // Try to_key first (receiver), then from_key (sender)
  auto& w = get_wallet(wallet_name);
  auto priv = w.find_private_key(to_key);
  if (priv)
    return crypto_.ecdh_decrypt(*priv, from_key, encrypted_content);
  priv = w.find_private_key(from_key);
  if (priv)
    return crypto_.ecdh_decrypt(*priv, to_key, encrypted_content);

  throw std::runtime_error("No matching private key found for ECDH decryption");
}

} // namespace beekeeper_minimal
