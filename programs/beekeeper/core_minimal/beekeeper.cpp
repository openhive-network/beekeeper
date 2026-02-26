#include <core_minimal/beekeeper.hpp>

namespace beekeeper_minimal {

beekeeper::beekeeper(crypto_provider& crypto, wallet_storage& storage,
                     uint32_t unlock_timeout_sec)
  : crypto_(crypto)
  , storage_(storage)
  , unlock_timeout_sec_(unlock_timeout_sec)
{
}

std::string beekeeper::generate_token(const std::string& salt)
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

std::string beekeeper::create_session(const std::string& salt)
{
  std::string token = generate_token(salt);

  while (sessions_.count(token))
    token = generate_token(salt);

  sessions_.emplace(token, session(token, unlock_timeout_sec_, crypto_, &storage_));
  return token;
}

void beekeeper::close_session(const std::string& token)
{
  sessions_.erase(token);
}

session& beekeeper::get_session(const std::string& token)
{
  auto it = sessions_.find(token);
  if (it == sessions_.end())
    throw std::runtime_error("Session not found: " + token);
  return it->second;
}

void beekeeper::check_timeouts()
{
  for (auto& [token, sess] : sessions_)
    sess.check_timeout();
}

} // namespace beekeeper_minimal
