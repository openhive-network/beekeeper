#include <core_minimal/beekeeper.hpp>

#include <random>
#include <sstream>

namespace beekeeper_minimal {

beekeeper::beekeeper(wallet_storage& storage, uint32_t unlock_timeout_sec)
  : storage_(storage)
  , unlock_timeout_sec_(unlock_timeout_sec)
{
}

std::string beekeeper::generate_token(const std::string& salt) const
{
  constexpr unsigned int token_length = 32;

  std::random_device rd;
  std::string seed_str = salt + std::to_string(rd());
  std::seed_seq seq(seed_str.begin(), seed_str.end());

  std::mt19937 gen;
  gen.seed(seq);

  std::uniform_int_distribution<> dis(0, 255);

  std::stringstream ss;
  for (unsigned int i = 0; i < token_length; ++i)
  {
    auto rc = static_cast<unsigned char>(dis(gen));
    std::stringstream hexstream;
    hexstream << std::hex << static_cast<int>(rc);
    auto hex = hexstream.str();
    ss << (hex.length() < 2 ? '0' + hex : hex);
  }
  return ss.str();
}

std::string beekeeper::create_session(const std::string& salt)
{
  std::string token = generate_token(salt);

  while (sessions_.count(token))
    token = generate_token(salt);

  sessions_.emplace(token, session(token, unlock_timeout_sec_, &storage_));
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
