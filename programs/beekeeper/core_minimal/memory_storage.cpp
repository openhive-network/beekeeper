#include <core_minimal/memory_storage.hpp>

#include <stdexcept>

namespace beekeeper_minimal {

void memory_storage::save(const std::string& name, const std::vector<char>& buffer)
{
  store_[name] = buffer;
}

std::vector<char> memory_storage::load(const std::string& name)
{
  auto it = store_.find(name);
  if (it == store_.end())
    throw std::runtime_error("Wallet not found: " + name);
  return it->second;
}

bool memory_storage::scan_dir(const std::string& wallet_name)
{
  return store_.count(wallet_name) != 0;
}

} // namespace beekeeper_minimal
