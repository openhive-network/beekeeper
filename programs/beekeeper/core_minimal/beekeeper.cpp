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
  , default_timeout_(unlock_timeout_sec)
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

  while (sessions_.count(token))
    token = generate_token();

  auto& session = sessions_[token];
  session.timeout = default_timeout_;
  refresh_timeout(session);
  return token;
}

void beekeeper::close_session(const std::string& token)
{
  auto it = sessions_.find(token);
  if (it == sessions_.end())
    return;

  auto wallet_names = std::move(it->second.wallets);
  sessions_.erase(it);

  for (auto& [name, unlocked] : wallet_names)
    release_wallet(name);
}

beekeeper::session_state& beekeeper::get_session(const std::string& token)
{
  auto it = sessions_.find(token);
  if (it == sessions_.end())
    throw std::runtime_error("Session not found: " + token);
  return it->second;
}

const beekeeper::session_state& beekeeper::get_session(const std::string& token) const
{
  auto it = sessions_.find(token);
  if (it == sessions_.end())
    throw std::runtime_error("Session not found: " + token);
  return it->second;
}

void beekeeper::validate_token(const std::string& token) const
{
  get_session(token);
}

// ── timeout ─────────────────────────────────────────────────

void beekeeper::set_timeout(const std::string& token, uint32_t seconds)
{
  auto& session = get_session(token);
  session.timeout = std::chrono::seconds(seconds);
  refresh_timeout(session);
}

void beekeeper::refresh_timeout(session_state& session)
{
  if (session.timeout.count() > 0)
    session.timeout_time.store((clock::now() + session.timeout).time_since_epoch().count());
}

void beekeeper::check_timeout()
{
  const auto now = clock::now().time_since_epoch().count();

  for (auto& [token, session] : sessions_)
  {
    if (session.timeout.count() > 0 && now >= session.timeout_time.load())
      lock_session_wallets(session);
  }
}

std::chrono::seconds beekeeper::get_remaining_seconds(const session_state& session) const
{
  if (session.timeout.count() == 0)
    return std::chrono::seconds::max();

  auto remaining = clock::time_point(clock::duration(session.timeout_time.load())) - clock::now();
  if (remaining.count() <= 0)
    return std::chrono::seconds(0);

  return std::chrono::duration_cast<std::chrono::seconds>(remaining);
}

session_info beekeeper::get_info(const std::string& token) const
{
  const auto& session = get_session(token);

  auto now = std::chrono::system_clock::now();
  auto now_t = std::chrono::system_clock::to_time_t(now);
  char buf[32];
  format_iso8601(buf, now_t);

  auto remaining = get_remaining_seconds(session);
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

wallet* beekeeper::session_wallet(const session_state& session, const std::string& wallet_name)
{
  auto it = session.wallets.find(wallet_name);
  if (it == session.wallets.end())
    throw std::runtime_error("Wallet not found: " + wallet_name);
  return it->second ? &get_wallet(wallet_name) : nullptr;
}

const wallet* beekeeper::session_wallet(const session_state& session, const std::string& wallet_name) const
{
  auto it = session.wallets.find(wallet_name);
  if (it == session.wallets.end())
    throw std::runtime_error("Wallet not found: " + wallet_name);
  return it->second ? &get_wallet(wallet_name) : nullptr;
}

void beekeeper::lock_session_wallets(session_state& session)
{
  for (auto& [name, unlocked] : session.wallets)
  {
    if (unlocked)
    {
      unlocked = false;
      release_wallet(name);
    }
  }
}

void beekeeper::release_wallet(const std::string& wallet_name)
{
  auto it = wallets_.find(wallet_name);
  if (it == wallets_.end())
    return;

  bool open = false;
  bool unlocked = false;
  for (const auto& [token, session] : sessions_)
  {
    auto entry = session.wallets.find(wallet_name);
    if (entry != session.wallets.end())
    {
      open = true;
      unlocked = unlocked || entry->second;
    }
  }

  if (!unlocked)
    it->second.lock();
  if (!open)
    wallets_.erase(it);
}

// ── wallet queries ──────────────────────────────────────────

bool beekeeper::has_wallet(const std::string& wallet_name) const
{
  if (wallets_.count(wallet_name))
    return true;

  return storage_.scan_dir(wallet_name);
}

std::vector<wallet_details> beekeeper::list_wallets(const std::string& token) const
{
  std::vector<wallet_details> result;

  auto it = sessions_.find(token);
  if (it != sessions_.end())
  {
    for (auto& [name, unlocked] : it->second.wallets)
      result.push_back({ name, unlocked });
  }

  return result;
}

// ── wallet operations ───────────────────────────────────────

std::string beekeeper::create_wallet(const std::string& token,
                                     const std::string& wallet_name,
                                     const std::string& password,
                                     bool is_temporary)
{
  refresh_timeout(get_session(token));

  if (wallets_.count(wallet_name))
    throw std::runtime_error("Wallet already exists: " + wallet_name);

  std::string pw = password.empty() ? gen_password() : password;

  // The wallet is added only once it is created (and opened below only once it is
  // loaded): a storage error must not leave a wallet no session holds. The WASM
  // build cannot rely on catch blocks for that cleanup, as an exception thrown by
  // a JS storage callback passes them by.
  wallet_storage* ws = is_temporary ? &mem_storage_ : &storage_;
  wallet w(crypto_, ws, wallet_name);
  w.create(pw);

  // In the WASM build other calls can run while the crypto callbacks are awaited
  // (gen_password, create): look the session up again instead of keeping a
  // reference across them, and never hand out a wallet created meanwhile.
  auto& session = get_session(token);
  if (!wallets_.emplace(wallet_name, std::move(w)).second)
    throw std::runtime_error("Wallet already exists: " + wallet_name);
  session.wallets[wallet_name] = true;

  return pw;
}

void beekeeper::open_wallet(const std::string& token, const std::string& wallet_name)
{
  auto& session = get_session(token);
  refresh_timeout(session);

  if (!wallets_.count(wallet_name))
  {
    // A wallet whose file cannot be loaded (e.g. deleted from disk) is not added,
    // so has_wallet() does not report it and create() can recreate it.
    wallet w(crypto_, &storage_, wallet_name);
    w.open();
    wallets_.emplace(wallet_name, std::move(w));
  }

  // Opened in this session, the wallet stays locked here until this session
  // unlocks it, also when another session has it unlocked.
  session.wallets.emplace(wallet_name, false);
}

void beekeeper::close_wallet(const std::string& token, const std::string& wallet_name)
{
  auto& session = get_session(token);
  refresh_timeout(session);

  if (session.wallets.erase(wallet_name))
    release_wallet(wallet_name);
}

void beekeeper::lock(const std::string& token, const std::string& wallet_name)
{
  auto& session = get_session(token);
  refresh_timeout(session);

  auto it = session.wallets.find(wallet_name);
  if (it == session.wallets.end())
    throw std::runtime_error("Wallet not found: " + wallet_name);

  if (it->second)
  {
    it->second = false;
    release_wallet(wallet_name);
  }
}

void beekeeper::lock_all(const std::string& token)
{
  lock_session_wallets(get_session(token));
}

void beekeeper::unlock(const std::string& token, const std::string& wallet_name, const std::string& password)
{
  auto& session = get_session(token);
  refresh_timeout(session);

  auto it = session.wallets.find(wallet_name);
  if (it == session.wallets.end())
    throw std::runtime_error("Wallet not found: " + wallet_name);
  if (it->second)
    throw std::runtime_error("Wallet is already unlocked: " + wallet_name);

  auto& w = get_wallet(wallet_name);
  if (w.is_locked())
    w.unlock(password);
  else
    w.check_password(password);  // unlocked by another session: its decrypted keys are kept as they are

  // In the WASM build other calls can run while the password is checked: find the
  // session's entry again instead of keeping an iterator across that.
  auto again = sessions_.find(token);
  if (again == sessions_.end() || !again->second.wallets.count(wallet_name))
  {
    release_wallet(wallet_name);
    throw std::runtime_error("Wallet not found: " + wallet_name);
  }
  again->second.wallets[wallet_name] = true;
}

// ── key operations ──────────────────────────────────────────

std::string beekeeper::import_key(const std::string& token,
                                  const std::string& wallet_name,
                                  const std::string& wif_key,
                                  const std::string& prefix,
                                  bool flush)
{
  auto& session = get_session(token);
  refresh_timeout(session);

  auto* w = session_wallet(session, wallet_name);
  if (!w)
    throw std::runtime_error("Wallet is locked: " + wallet_name);
  return w->import_key(wif_key, prefix, flush);
}


void beekeeper::remove_key(const std::string& token,
                           const std::string& wallet_name,
                           const public_key_type& public_key)
{
  auto& session = get_session(token);
  refresh_timeout(session);

  auto* w = session_wallet(session, wallet_name);
  if (!w)
    throw std::runtime_error("Wallet is locked: " + wallet_name);
  w->remove_key(public_key);
}

keys_map beekeeper::get_public_keys(const std::string& token, const std::string& wallet_name) const
{
  const auto* w = session_wallet(get_session(token), wallet_name);
  if (!w)
    throw std::runtime_error("Wallet is locked: " + wallet_name);
  return w->get_keys();
}

bool beekeeper::has_private_key(const std::string& token,
                                const std::string& wallet_name,
                                const public_key_type& public_key) const
{
  const auto* w = session_wallet(get_session(token), wallet_name);
  return w && w->has_private_key(public_key);
}

// ── signing ─────────────────────────────────────────────────

signature_type beekeeper::sign_digest(const std::string& token,
                                      const std::string& wallet_name,
                                      const digest_type& digest,
                                      const public_key_type& public_key,
                                      const std::string& prefix)
{
  auto& session = get_session(token);
  refresh_timeout(session);

  if (const auto* w = session_wallet(session, wallet_name))
  {
    auto sig = w->try_sign_digest(digest, public_key);
    if (sig)
      return *sig;
  }
  throw std::runtime_error("Public key " + crypto_.public_key_to_string(public_key, prefix) +
                           " not found in wallet " + wallet_name);
}

// ── encrypt / decrypt ───────────────────────────────────────

std::string beekeeper::encrypt_data(const std::string& token,
                                    const std::string& wallet_name,
                                    const public_key_type& from_key,
                                    const public_key_type& to_key,
                                    const std::string& content,
                                    const std::string& prefix,
                                    uint64_t nonce)
{
  auto& session = get_session(token);
  refresh_timeout(session);

  std::optional<private_key_type> priv;
  if (const auto* w = session_wallet(session, wallet_name))
    priv = w->find_private_key(from_key);
  if (!priv)
    throw std::runtime_error("Public key " + crypto_.public_key_to_string(from_key, prefix) +
                             " not found in wallet " + wallet_name);

  std::optional<uint64_t> nonce_opt;
  if (nonce != 0)
    nonce_opt = nonce;

  return crypto_.ecdh_encrypt(*priv, to_key, content, nonce_opt);
}

std::string beekeeper::decrypt_data(const std::string& token,
                                    const std::string& wallet_name,
                                    const public_key_type& from_key,
                                    const public_key_type& to_key,
                                    const std::string& encrypted_content,
                                    const std::string& prefix)
{
  auto& session = get_session(token);
  refresh_timeout(session);

  // Try to_key first (receiver), then from_key (sender)
  if (const auto* w = session_wallet(session, wallet_name))
  {
    auto priv = w->find_private_key(to_key);
    if (priv)
      return crypto_.ecdh_decrypt(*priv, from_key, encrypted_content);
    priv = w->find_private_key(from_key);
    if (priv)
      return crypto_.ecdh_decrypt(*priv, to_key, encrypted_content);
  }

  throw std::runtime_error("No matching private key found for ECDH decryption");
}

} // namespace beekeeper_minimal
