#pragma once

#include <core_minimal/wallet_storage.hpp>

#include <map>

namespace beekeeper_minimal {

/// In-memory wallet_storage: stores wallet blobs in a std::map.
/// Used for temporary wallets that should not persist to disk/IDB.
class memory_storage final : public wallet_storage
{
public:
  void save(const std::string& name, const std::vector<char>& buffer) override;
  std::vector<char> load(const std::string& name) override;
  std::vector<std::string> list_dir() override;

private:
  std::map<std::string, std::vector<char>> store_;
};

} // namespace beekeeper_minimal
