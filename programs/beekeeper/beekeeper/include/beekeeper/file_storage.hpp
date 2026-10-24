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

  /// Wallet names consist of ASCII letters, digits and '.', '_', '-', '@', so {name}.wallet
  /// always names a file directly in the wallet directory. save() and load() refuse other
  /// names, scan_dir() reports no wallet for them.
  static bool is_valid_name(const std::string& name);

  void save(const std::string& name, const std::vector<char>& buffer) override;
  std::vector<char> load(const std::string& name) override;
  bool scan_dir(const std::string& wallet_name) override;

  /// Return names of all wallets currently in storage (native daemon only).
  std::vector<std::string> list_dir();

private:
  boost::filesystem::path wallet_dir_;

  boost::filesystem::path wallet_path(const std::string& name) const;
};

} // namespace beekeeper
