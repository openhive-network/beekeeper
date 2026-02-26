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

std::vector<std::string> memory_storage::list_dir()
{
  std::vector<std::string> names;
  names.reserve(store_.size());
  for (const auto& [name, _] : store_)
    names.push_back(name);
  return names;
}

} // namespace beekeeper_minimal
