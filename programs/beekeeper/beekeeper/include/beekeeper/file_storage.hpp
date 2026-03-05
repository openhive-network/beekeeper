#pragma once

#include <core_minimal/wallet_storage.hpp>

#include <boost/filesystem/path.hpp>

namespace beekeeper {

/// Implements wallet_storage using Boost.Filesystem.
/// Wallets are stored as {wallet_dir}/{name}.wallet files.
class file_storage final : public beekeeper_minimal::wallet_storage
{
public:
  explicit file_storage(const boost::filesystem::path& wallet_dir);

  void save(const std::string& name, const std::vector<char>& buffer) override;
  std::vector<char> load(const std::string& name) override;
  std::vector<std::string> list_dir() override;

private:
  boost::filesystem::path wallet_dir_;

  boost::filesystem::path wallet_path(const std::string& name) const;
};

} // namespace beekeeper
